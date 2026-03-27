#include "core/comm_manager.h"
#include "Arduino.h"
#include "config.h"
#include <cstring>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(FT_PLATFORM_PC)
#include <SDL2/SDL.h>
#endif

#if defined(__linux__)
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

ConnectionType CommManager::currentType = CONN_WIRED;
MachineConfig CommManager::currentConfig = {};
bool CommManager::initialized = false;
MessageCallback CommManager::messageCallback = nullptr;
MessageCallback CommManager::terminalCallback = nullptr;
CommManager::CommandTap CommManager::commandTap = nullptr;
CommManager::EventCallback CommManager::eventCallback = nullptr;
bool CommManager::jog_soft_limit_x_enabled = false;
bool CommManager::jog_soft_limit_y_enabled = false;
bool CommManager::jog_soft_limit_z_enabled = false;
float CommManager::jog_soft_limit_x_min = 0.0f;
float CommManager::jog_soft_limit_x_max = 0.0f;
float CommManager::jog_soft_limit_y_min = 0.0f;
float CommManager::jog_soft_limit_y_max = 0.0f;
float CommManager::jog_soft_limit_z_min = 0.0f;
float CommManager::jog_soft_limit_z_max = 0.0f;
bool CommManager::jog_pending_valid = false;
float CommManager::jog_pending_x = 0.0f;
float CommManager::jog_pending_y = 0.0f;
float CommManager::jog_pending_z = 0.0f;
int32_t CommManager::jog_pending_command_count = 0;
float CommManager::jog_last_reported_x = 0.0f;
float CommManager::jog_last_reported_y = 0.0f;
float CommManager::jog_last_reported_z = 0.0f;
MachineState CommManager::jog_last_state = STATE_DISCONNECTED;

namespace {
bool g_connected = false;
bool g_auto_reporting = false;
FluidNCStatus g_status;

#if defined(__linux__)
int g_serial_fd = -1;
uint32_t g_last_status_request_ms = 0;
uint32_t g_last_status_rx_ms = 0;
char g_line_buffer[256] = {0};
size_t g_line_len = 0;
MessageCallback g_message_callback = nullptr;
MessageCallback g_terminal_callback = nullptr;
constexpr uint32_t kStatusTimeoutMs = 5000;
constexpr uint8_t kCmdMpgModeToggle = 0x8B;
bool g_mpg_mode_requested = false;
constexpr size_t kMacroMaxInFlight = 4;
std::thread g_macro_thread;
std::mutex g_macro_mutex;
std::condition_variable g_macro_cv;
bool g_macro_running = false;
bool g_macro_stop_requested = false;
size_t g_macro_in_flight = 0;
size_t g_macro_total_lines = 0;
size_t g_macro_sent_lines = 0;
size_t g_macro_acked_lines = 0;
uint32_t g_macro_start_ms = 0;

void updateLocalMacroStatusLocked()
{
    if (g_macro_total_lines == 0) {
        g_status.sd_percent = 0.0f;
    } else {
        g_status.sd_percent = (100.0f * static_cast<float>(g_macro_acked_lines)) / static_cast<float>(g_macro_total_lines);
    }
    if (g_status.sd_percent > 100.0f) {
        g_status.sd_percent = 100.0f;
    }
    g_status.is_sd_printing = true;
    g_status.sd_elapsed_ms = millis() - g_macro_start_ms;
    snprintf(g_status.last_message, sizeof(g_status.last_message),
             "Local macro %u/%u",
             static_cast<unsigned>(g_macro_acked_lines),
             static_cast<unsigned>(g_macro_total_lines));
}

speed_t baudToTermios(uint32_t baud)
{
    switch (baud) {
        case 115200: return B115200;
#ifdef B230400
        case 230400: return B230400;
#endif
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B500000
        case 500000: return B500000;
#endif
#ifdef B576000
        case 576000: return B576000;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default: return B115200;
    }
}

void closeSerial()
{
    if (g_serial_fd >= 0) {
        close(g_serial_fd);
        g_serial_fd = -1;
    }
    g_mpg_mode_requested = false;
    g_line_len = 0;
    g_last_status_request_ms = 0;
    g_last_status_rx_ms = 0;
}

void writeTransportOnly(const char *command)
{
    if ((CommManager::getConnectionType() == CONN_WIRED || CommManager::getConnectionType() == CONN_UART) && g_serial_fd >= 0) {
        write(g_serial_fd, command, strlen(command));
    }
}

void stopLocalMacroRunner(bool wait_for_join)
{
    std::thread worker_to_join;
    {
        std::lock_guard<std::mutex> lock(g_macro_mutex);
        if (!g_macro_running && !g_macro_thread.joinable()) {
            return;
        }
        g_macro_stop_requested = true;
        g_status.is_sd_printing = false;
        g_status.sd_elapsed_ms = 0;
        if (g_macro_running) {
            snprintf(g_status.last_message, sizeof(g_status.last_message), "Local macro stopped");
        }
        g_macro_cv.notify_all();
        if (wait_for_join && g_macro_thread.joinable()) {
            worker_to_join = std::move(g_macro_thread);
        }
    }
    if (worker_to_join.joinable()) {
        worker_to_join.join();
    }
}

bool openSerial(const char *path, uint32_t baud)
{
    closeSerial();
    if (!path || path[0] == '\0') {
        return false;
    }

    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    termios tio{};
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return false;
    }

    cfmakeraw(&tio);
    const speed_t speed = baudToTermios(baud);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return false;
    }

    tcflush(fd, TCIOFLUSH);
    g_serial_fd = fd;
    g_mpg_mode_requested = false;
    return true;
}

void requestMpgMode(bool enable)
{
    if (g_serial_fd < 0) {
        g_mpg_mode_requested = false;
        return;
    }
    if (g_mpg_mode_requested == enable) {
        return;
    }
    const uint8_t cmd = kCmdMpgModeToggle;
    if (write(g_serial_fd, &cmd, 1) == 1) {
        g_mpg_mode_requested = enable;
    }
}

void parseGCodeState(const char *message)
{
    if (!message || strncmp(message, "[GC:", 4) != 0) {
        return;
    }

    char gcodes[160] = {0};
    const int matched = sscanf(message, "[GC:%159[^]]", gcodes);
    if (matched != 1) {
        return;
    }

    char *token = strtok(gcodes, " ");
    while (token) {
        if (token[0] == 'G') {
            if (strcmp(token, "G54") == 0 || strcmp(token, "G55") == 0 ||
                strcmp(token, "G56") == 0 || strcmp(token, "G57") == 0 ||
                strcmp(token, "G58") == 0 || strcmp(token, "G59") == 0) {
                strncpy(g_status.modal_wcs, token, sizeof(g_status.modal_wcs) - 1);
            } else if (strcmp(token, "G17") == 0 || strcmp(token, "G18") == 0 || strcmp(token, "G19") == 0) {
                strncpy(g_status.modal_plane, token, sizeof(g_status.modal_plane) - 1);
            } else if (strcmp(token, "G90") == 0 || strcmp(token, "G91") == 0) {
                strncpy(g_status.modal_distance, token, sizeof(g_status.modal_distance) - 1);
            } else if (strcmp(token, "G20") == 0 || strcmp(token, "G21") == 0) {
                strncpy(g_status.modal_units, token, sizeof(g_status.modal_units) - 1);
            } else if (strcmp(token, "G0") == 0 || strcmp(token, "G1") == 0 ||
                       strcmp(token, "G2") == 0 || strcmp(token, "G3") == 0 ||
                       strcmp(token, "G38.2") == 0 || strcmp(token, "G80") == 0) {
                strncpy(g_status.modal_motion, token, sizeof(g_status.modal_motion) - 1);
            } else if (strcmp(token, "G93") == 0 || strcmp(token, "G94") == 0) {
                strncpy(g_status.modal_feedrate, token, sizeof(g_status.modal_feedrate) - 1);
            }
        } else if (token[0] == 'M') {
            if (strcmp(token, "M3") == 0 || strcmp(token, "M4") == 0 || strcmp(token, "M5") == 0) {
                strncpy(g_status.modal_spindle, token, sizeof(g_status.modal_spindle) - 1);
            } else if (strcmp(token, "M7") == 0 || strcmp(token, "M8") == 0 || strcmp(token, "M9") == 0) {
                strncpy(g_status.modal_coolant, token, sizeof(g_status.modal_coolant) - 1);
            }
        } else if (token[0] == 'T') {
            strncpy(g_status.modal_tool, token, sizeof(g_status.modal_tool) - 1);
        }
        token = strtok(nullptr, " ");
    }
}

void parseStatusReport(const char *message)
{
    if (!message || message[0] != '<') {
        return;
    }

    g_last_status_rx_ms = millis();
    g_status.last_update_ms = g_last_status_rx_ms;

    if (!g_status.is_connected) {
        g_status.is_connected = true;
        CommManager::emitConnected();
    }

    MachineState newState = g_status.state;
    if (strstr(message, "<Idle")) newState = STATE_IDLE;
    else if (strstr(message, "<Run")) newState = STATE_RUN;
    else if (strstr(message, "<Hold")) newState = STATE_HOLD;
    else if (strstr(message, "<Jog")) newState = STATE_JOG;
    else if (strstr(message, "<Alarm")) newState = STATE_ALARM;
    else if (strstr(message, "<Door")) newState = STATE_DOOR;
    else if (strstr(message, "<Check")) newState = STATE_CHECK;
    else if (strstr(message, "<Home")) newState = STATE_HOME;
    else if (strstr(message, "<Sleep")) newState = STATE_SLEEP;
    g_status.state = newState;

    const char *mpos = strstr(message, "MPos:");
    if (mpos) {
        sscanf(mpos + 5, "%f,%f,%f", &g_status.mpos_x, &g_status.mpos_y, &g_status.mpos_z);
    }

    const char *wco = strstr(message, "WCO:");
    if (wco) {
        sscanf(wco + 4, "%f,%f,%f", &g_status.wco_x, &g_status.wco_y, &g_status.wco_z);
    }

    g_status.wpos_x = g_status.mpos_x - g_status.wco_x;
    g_status.wpos_y = g_status.mpos_y - g_status.wco_y;
    g_status.wpos_z = g_status.mpos_z - g_status.wco_z;

    const char *wpos = strstr(message, "WPos:");
    if (wpos) {
        sscanf(wpos + 5, "%f,%f,%f", &g_status.wpos_x, &g_status.wpos_y, &g_status.wpos_z);
    }

    const char *fs = strstr(message, "FS:");
    if (fs) {
        sscanf(fs + 3, "%f,%f", &g_status.feed_rate, &g_status.spindle_speed);
    }

    const char *ov = strstr(message, "Ov:");
    if (ov) {
        sscanf(ov + 3, "%f,%f,%f", &g_status.feed_override, &g_status.rapid_override, &g_status.spindle_override);
    }

    const char *mpg = strstr(message, "MPG:");
    if (mpg) {
        int mpg_status = -1;
        if (sscanf(mpg + 4, "%d", &mpg_status) == 1) {
            if (mpg_status == 0) {
                // Re-arm and re-request so grblHAL keeps MPG enabled.
                g_mpg_mode_requested = false;
                requestMpgMode(true);
            } else {
                g_mpg_mode_requested = true;
            }
        }
    }

    bool local_macro_running = false;
    {
        std::lock_guard<std::mutex> lock(g_macro_mutex);
        local_macro_running = g_macro_running;
    }
    if (!local_macro_running) {
        const char *sd = strstr(message, "SD:");
        if (sd) {
            float pct = 0.0f;
            uint32_t elapsed = 0;
            if (sscanf(sd + 3, "%f,%u", &pct, &elapsed) >= 1) {
                g_status.is_sd_printing = true;
                g_status.sd_percent = pct;
                g_status.sd_elapsed_ms = elapsed * 1000;
            }
        } else {
            g_status.is_sd_printing = false;
            g_status.sd_percent = 0.0f;
            g_status.sd_elapsed_ms = 0;
        }
    }
}

void handleLine(const char *line)
{
    if (!line || line[0] == '\0') {
        return;
    }

    if (g_message_callback) {
        g_message_callback(line);
    }

    if (line[0] == '<') {
        parseStatusReport(line);
        return;
    }

    if (strncmp(line, "[GC:", 4) == 0) {
        parseGCodeState(line);
    } else if (strncmp(line, "[MSG:", 5) == 0) {
        sscanf(line, "[MSG:%127[^]]", g_status.last_message);
    } else if (strncmp(line, "error:", 6) == 0 || strncmp(line, "ALARM:", 6) == 0) {
        strncpy(g_status.last_message, line, sizeof(g_status.last_message) - 1);
    }

    if (g_terminal_callback) {
        g_terminal_callback(line);
    }

    if (strcmp(line, "ok") == 0 || strncmp(line, "error:", 6) == 0 || strncmp(line, "ALARM:", 6) == 0) {
        std::lock_guard<std::mutex> lock(g_macro_mutex);
        if (g_macro_in_flight > 0) {
            --g_macro_in_flight;
            if (g_macro_acked_lines < g_macro_total_lines) {
                ++g_macro_acked_lines;
            }
            if (g_macro_running) {
                updateLocalMacroStatusLocked();
            }
            g_macro_cv.notify_all();
        }
    }
}
#endif

std::string trimLine(const std::string &line)
{
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t' || line[start] == '\r' || line[start] == '\n')) {
        ++start;
    }
    size_t end = line.size();
    while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r' || line[end - 1] == '\n')) {
        --end;
    }
    return line.substr(start, end - start);
}

bool startsWith(const char *text, const char *prefix)
{
    if (!text || !prefix) {
        return false;
    }
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

bool extractRunRequest(const char *command, std::string &out_path, int &out_repeat_count)
{
    out_repeat_count = 1;
    static constexpr const char *kRunPrefixes[] = {"$LocalFS/Run=", "$SD/Run="};
    for (const char *prefix : kRunPrefixes) {
        if (startsWith(command, prefix)) {
            std::string payload = trimLine(command + strlen(prefix));
            if (startsWith(payload.c_str(), "Repeat=")) {
                const size_t sep = payload.find('|');
                if (sep == std::string::npos || sep <= strlen("Repeat=") || sep + 1 >= payload.length()) {
                    return false;
                }
                int repeat = atoi(payload.substr(strlen("Repeat="), sep - strlen("Repeat=")).c_str());
                if (repeat < 1) {
                    return false;
                }
                out_repeat_count = repeat;
                out_path = trimLine(payload.substr(sep + 1));
                return !out_path.empty();
            }
            out_path = payload;
            return !out_path.empty();
        }
    }

    static constexpr const char *kRepeatPrefixes[] = {"$LocalFS/RunRepeat=", "$SD/RunRepeat="};
    for (const char *prefix : kRepeatPrefixes) {
        if (!startsWith(command, prefix)) {
            continue;
        }
        std::string payload = trimLine(command + strlen(prefix));
        const size_t sep = payload.find('|');
        if (sep == std::string::npos || sep == 0 || sep + 1 >= payload.length()) {
            return false;
        }
        int repeat = atoi(payload.substr(0, sep).c_str());
        if (repeat < 1) {
            return false;
        }
        out_repeat_count = repeat;
        out_path = trimLine(payload.substr(sep + 1));
        return !out_path.empty();
    }
    return false;
}

bool isStopLocalMacroCommand(const char *command)
{
    if (!command) {
        return false;
    }
    const std::string trimmed = trimLine(command);
    return trimmed == "$LocalFS/Stop" || trimmed == "$SD/Stop" || trimmed == "$Macro/Stop";
}

bool shouldAbortLocalMacroOnCommand(const char *command)
{
    if (!command) {
        return false;
    }
    if (command[0] == 0x18) {  // Soft reset
        return true;
    }
    const std::string trimmed = trimLine(command);
    return trimmed == "!" || trimmed == "\x18";
}

std::string resolveMacroPath(const std::string &macro_path)
{
    if (macro_path.empty()) {
        return std::string();
    }

    std::vector<std::string> candidates;
    candidates.push_back(macro_path);

    if (!macro_path.empty() && macro_path[0] == '/') {
        candidates.push_back("." + macro_path);
    }

#if defined(FT_PLATFORM_PC)
    char *sdl_base = SDL_GetBasePath();
    if (sdl_base && sdl_base[0] != '\0') {
        std::string base = sdl_base;
        if (!base.empty() && base.back() == '/') {
            base.pop_back();
        }
        if (!macro_path.empty() && macro_path[0] == '/') {
            candidates.push_back(base + macro_path);
        } else {
            candidates.push_back(base + "/" + macro_path);
        }
    }
    if (sdl_base) {
        SDL_free(sdl_base);
    }
#endif

    for (const std::string &candidate : candidates) {
        std::ifstream test(candidate.c_str(), std::ios::in);
        if (test.is_open()) {
            return candidate;
        }
    }

    return std::string();
}

void sendTransportCommand(const char *command, bool echo_terminal)
{
    writeTransportOnly(command);
    if (echo_terminal && g_terminal_callback) {
        g_terminal_callback(command);
    }
}

bool startLocalMacroRunner(const std::string &resolved_path, int repeat_count)
{
    if (repeat_count < 1) {
        return false;
    }
    bool should_join_finished = false;
    {
        std::lock_guard<std::mutex> lock(g_macro_mutex);
        should_join_finished = (!g_macro_running && g_macro_thread.joinable());
    }
    if (should_join_finished) {
        g_macro_thread.join();
    }

    std::string display_name = resolved_path;
    const size_t slash_pos = display_name.find_last_of('/');
    if (slash_pos != std::string::npos && slash_pos + 1 < display_name.size()) {
        display_name = display_name.substr(slash_pos + 1);
    }

    std::vector<std::string> lines;
    std::ifstream macro_file(resolved_path.c_str(), std::ios::in);
    if (!macro_file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(macro_file, line)) {
        const std::string trimmed = trimLine(line);
        if (trimmed.empty() || trimmed[0] == ';') {
            continue;
        }
        lines.push_back(trimmed + "\n");
    }

    {
        std::lock_guard<std::mutex> lock(g_macro_mutex);
        if (g_macro_running || g_macro_thread.joinable()) {
            return false;
        }
        g_macro_running = true;
        g_macro_stop_requested = false;
        g_macro_in_flight = 0;
        g_macro_total_lines = lines.size() * static_cast<size_t>(repeat_count);
        g_macro_sent_lines = 0;
        g_macro_acked_lines = 0;
        g_macro_start_ms = millis();
        g_status.is_sd_printing = true;
        g_status.sd_start_time_ms = g_macro_start_ms;
        g_status.sd_elapsed_ms = 0;
        strncpy(g_status.sd_filename, display_name.c_str(), sizeof(g_status.sd_filename) - 1);
        g_status.sd_filename[sizeof(g_status.sd_filename) - 1] = '\0';
        updateLocalMacroStatusLocked();
    }

    g_macro_thread = std::thread([lines = std::move(lines), repeat_count]() {
        for (int pass = 0; pass < repeat_count; ++pass) {
            for (const std::string &gcode : lines) {
            std::unique_lock<std::mutex> lock(g_macro_mutex);
            g_macro_cv.wait(lock, []() {
                return g_macro_stop_requested || !g_connected || g_macro_in_flight < kMacroMaxInFlight;
            });
            if (g_macro_stop_requested || !g_connected) {
                break;
            }
            ++g_macro_in_flight;
            if (g_macro_sent_lines < g_macro_total_lines) {
                ++g_macro_sent_lines;
            }
            updateLocalMacroStatusLocked();
            lock.unlock();
            writeTransportOnly(gcode.c_str());
        }
            std::unique_lock<std::mutex> lock(g_macro_mutex);
            if (g_macro_stop_requested || !g_connected) {
                break;
            }
        }

        std::unique_lock<std::mutex> lock(g_macro_mutex);
        g_macro_cv.wait(lock, []() {
            return g_macro_stop_requested || !g_connected || g_macro_in_flight == 0;
        });
        g_macro_running = false;
        g_macro_stop_requested = false;
        g_macro_in_flight = 0;
        g_macro_total_lines = 0;
        g_macro_sent_lines = 0;
        g_macro_acked_lines = 0;
        g_status.is_sd_printing = false;
        g_status.sd_elapsed_ms = 0;
        snprintf(g_status.last_message, sizeof(g_status.last_message), "Local macro complete");
    });
    return true;
}
}

void CommManager::init() {
    initialized = true;
    g_connected = false;
    g_auto_reporting = false;
    g_status = FluidNCStatus();
    resetJogTracking();
}

bool CommManager::connect(const MachineConfig &config) {
    currentConfig = config;
    currentType = config.connection_type;
    setJogSoftLimits(config.soft_limit_x_enabled,
                     config.soft_limit_y_enabled,
                     config.soft_limit_z_enabled,
                     config.soft_limit_x_min,
                     config.soft_limit_x_max,
                     config.soft_limit_y_min,
                     config.soft_limit_y_max,
                     config.soft_limit_z_min,
                     config.soft_limit_z_max);
    resetJogTracking();

#if defined(__linux__)
    if (currentType == CONN_WIRED || currentType == CONN_UART) {
        if (config.serial_port[0] == '\0') {
            emitConnectionError("No serial port selected");
            return false;
        }
        uint32_t baud = config.uart_baudrate;
        if (baud < 115200 || baud > 921600) {
            baud = GRBL_UART_BAUD;
        }
        if (!openSerial(config.serial_port, baud)) {
            emitConnectionError("Failed to open selected serial port");
            return false;
        }
        g_connected = true;
        g_auto_reporting = false;
        g_status = FluidNCStatus();
        g_status.state = STATE_DISCONNECTED;
        g_status.is_connected = false;
        g_status.last_update_ms = millis();
        requestMpgMode(true);
        updateJogTrackingFromStatus(g_status);
        return true;
    }
#endif

    g_connected = true;
    g_status.is_connected = true;
    g_status.state = STATE_IDLE;
    g_status.last_update_ms = millis();
    updateJogTrackingFromStatus(g_status);
    emitConnected();
    return true;
}

void CommManager::disconnect() {
    stopLocalMacroRunner(true);
    resetJogTracking();
#if defined(__linux__)
    requestMpgMode(false);
    closeSerial();
#endif
    const bool was_connected = g_connected || g_status.is_connected;
    g_connected = false;
    g_status.is_connected = false;
    g_status.state = STATE_DISCONNECTED;
    if (was_connected) {
        emitDisconnected("Disconnected");
    }
}

void CommManager::stopReconnectionAttempts() {}

bool CommManager::isConnected() {
#if defined(__linux__)
    if (currentType == CONN_WIRED || currentType == CONN_UART) {
        return g_status.is_connected;
    }
#endif
    return g_connected;
}

bool CommManager::isAutoReporting() {
    return g_auto_reporting;
}

void CommManager::loop() {
    if (!g_connected) {
        return;
    }

#if defined(__linux__)
    if (currentType == CONN_WIRED || currentType == CONN_UART) {
        if (g_serial_fd < 0) {
            g_connected = false;
            g_status.is_connected = false;
            g_status.state = STATE_DISCONNECTED;
            emitDisconnected("Serial port closed");
            return;
        }

        char rx[128];
        while (true) {
            const ssize_t n = read(g_serial_fd, rx, sizeof(rx));
            if (n > 0) {
                for (ssize_t i = 0; i < n; ++i) {
                    const char ch = rx[i];
                    if (ch == '\n' || ch == '\r') {
                        if (g_line_len > 0) {
                            g_line_buffer[g_line_len] = '\0';
                            handleLine(g_line_buffer);
                            g_line_len = 0;
                        }
                    } else if (g_line_len < sizeof(g_line_buffer) - 1) {
                        g_line_buffer[g_line_len++] = ch;
                    }
                }
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            if (n < 0) {
                closeSerial();
                g_connected = false;
                g_status.is_connected = false;
                g_status.state = STATE_DISCONNECTED;
                emitDisconnected("Serial read failure");
            }
            break;
        }

        const uint32_t now = millis();
        if ((now - g_last_status_request_ms) >= 250) {
            const char q = '?';
            write(g_serial_fd, &q, 1);
            g_last_status_request_ms = now;
        }

        if (g_status.is_connected && g_last_status_rx_ms != 0 && (now - g_last_status_rx_ms) > kStatusTimeoutMs) {
            g_status.is_connected = false;
            g_status.state = STATE_DISCONNECTED;
            emitDisconnected("Status timeout");
        }
        updateJogTrackingFromStatus(g_status);
        return;
    }
#endif

    g_status.last_update_ms = millis();
    updateJogTrackingFromStatus(g_status);
}

const FluidNCStatus &CommManager::getStatus() {
    return g_status;
}

void CommManager::sendCommand(const char *command) {
    if (!command) {
        return;
    }
    if (commandTap) {
        commandTap(command);
    }

    if (isStopLocalMacroCommand(command)) {
        stopLocalMacroRunner(false);
        return;
    }

    if (shouldAbortLocalMacroOnCommand(command)) {
        stopLocalMacroRunner(false);
    }

    std::string macro_path;
    int repeat_count = 1;
    if (extractRunRequest(command, macro_path, repeat_count)) {
        const std::string resolved = resolveMacroPath(macro_path);
        if (resolved.empty()) {
            char msg[192];
            snprintf(msg, sizeof(msg), "[PC] Macro file not found: %s", macro_path.c_str());
            emitConnectionError(msg);
            return;
        }

        if (!startLocalMacroRunner(resolved, repeat_count)) {
            char msg[224];
            snprintf(msg, sizeof(msg), "[PC] Failed to start local macro runner: %s", macro_path.c_str());
            emitConnectionError(msg);
            return;
        }
        return;
    }

    sendTransportCommand(command, true);
}

bool CommManager::sendJog(const JogCommand &cmd) {
    bool transport_ready = g_connected;
#if defined(__linux__)
    if (currentType == CONN_WIRED || currentType == CONN_UART) {
        transport_ready = (g_serial_fd >= 0);
    }
#endif
    if (!transport_ready) {
        return false;
    }

    if (!cmd.has_x && !cmd.has_y && !cmd.has_z) {
        return false;
    }

    const FluidNCStatus &status = getStatus();
    float predicted_x = 0.0f;
    float predicted_y = 0.0f;
    float predicted_z = 0.0f;
    getPredictedWpos(status, predicted_x, predicted_y, predicted_z);

    bool include_x = cmd.has_x;
    bool include_y = cmd.has_y;
    bool include_z = cmd.has_z;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float target_z = 0.0f;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    float delta_z = 0.0f;

    if (include_x) {
        target_x = cmd.absolute ? cmd.x : (predicted_x + cmd.x);
        target_x = clampJogTarget(predicted_x, target_x, jog_soft_limit_x_enabled, jog_soft_limit_x_min, jog_soft_limit_x_max);
        delta_x = target_x - predicted_x;
        if (delta_x == 0.0f) include_x = false;
    }
    if (include_y) {
        target_y = cmd.absolute ? cmd.y : (predicted_y + cmd.y);
        target_y = clampJogTarget(predicted_y, target_y, jog_soft_limit_y_enabled, jog_soft_limit_y_min, jog_soft_limit_y_max);
        delta_y = target_y - predicted_y;
        if (delta_y == 0.0f) include_y = false;
    }
    if (include_z) {
        target_z = cmd.absolute ? cmd.z : (predicted_z + cmd.z);
        target_z = clampJogTarget(predicted_z, target_z, jog_soft_limit_z_enabled, jog_soft_limit_z_min, jog_soft_limit_z_max);
        delta_z = target_z - predicted_z;
        if (delta_z == 0.0f) include_z = false;
    }

    if (!include_x && !include_y && !include_z) {
        return false;
    }

    char jog_cmd[128];
    size_t len = 0;
    len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, "$J=");
    if (cmd.units && cmd.units[0] != '\0') {
        len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, "%s", cmd.units);
    }
    len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, "%s", cmd.absolute ? "G90" : "G91");
    if (include_x) {
        float value = cmd.absolute ? target_x : delta_x;
        len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, " X%.4f", value);
    }
    if (include_y) {
        float value = cmd.absolute ? target_y : delta_y;
        len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, " Y%.4f", value);
    }
    if (include_z) {
        float value = cmd.absolute ? target_z : delta_z;
        len += snprintf(jog_cmd + len, sizeof(jog_cmd) - len, " Z%.4f", value);
    }
    snprintf(jog_cmd + len, sizeof(jog_cmd) - len, " F%.0f\n", cmd.feedrate);
    sendCommand(jog_cmd);

    if (jog_pending_valid) {
        if (include_x) jog_pending_x += (cmd.absolute ? (target_x - predicted_x) : delta_x);
        if (include_y) jog_pending_y += (cmd.absolute ? (target_y - predicted_y) : delta_y);
        if (include_z) jog_pending_z += (cmd.absolute ? (target_z - predicted_z) : delta_z);
        jog_pending_command_count++;
    }

    return true;
}

bool CommManager::sendJogRelative(float x, float y, float z, float feedrate) {
    JogCommand cmd{};
    cmd.absolute = false;
    cmd.has_x = (x != 0.0f);
    cmd.has_y = (y != 0.0f);
    cmd.has_z = (z != 0.0f);
    cmd.x = x;
    cmd.y = y;
    cmd.z = z;
    cmd.feedrate = feedrate;
    cmd.units = nullptr;
    return sendJog(cmd);
}

bool CommManager::sendJogRelativeAxis(char axis, float delta, float feedrate) {
    JogCommand cmd{};
    cmd.absolute = false;
    cmd.has_x = (axis == 'X' || axis == 'x');
    cmd.has_y = (axis == 'Y' || axis == 'y');
    cmd.has_z = (axis == 'Z' || axis == 'z');
    cmd.x = cmd.has_x ? delta : 0.0f;
    cmd.y = cmd.has_y ? delta : 0.0f;
    cmd.z = cmd.has_z ? delta : 0.0f;
    cmd.feedrate = feedrate;
    cmd.units = nullptr;
    return sendJog(cmd);
}

bool CommManager::getJogPredictedWpos(float &x, float &y, float &z) {
    const FluidNCStatus &status = getStatus();
    getPredictedWpos(status, x, y, z);
    return jog_pending_valid && status.is_connected;
}

int32_t CommManager::getJogPendingCommandCount() {
    return jog_pending_command_count;
}

void CommManager::setJogSoftLimits(bool x_enabled, bool y_enabled, bool z_enabled,
                                   float x_min, float x_max,
                                   float y_min, float y_max,
                                   float z_min, float z_max) {
    jog_soft_limit_x_enabled = x_enabled;
    jog_soft_limit_y_enabled = y_enabled;
    jog_soft_limit_z_enabled = z_enabled;
    jog_soft_limit_x_min = x_min;
    jog_soft_limit_x_max = x_max;
    jog_soft_limit_y_min = y_min;
    jog_soft_limit_y_max = y_max;
    jog_soft_limit_z_min = z_min;
    jog_soft_limit_z_max = z_max;

    const FluidNCStatus &status = getStatus();
    resetJogTracking();
    updateJogTrackingFromStatus(status);
}

void CommManager::requestStatusReport() {
#if defined(__linux__)
    if ((currentType == CONN_WIRED || currentType == CONN_UART) && g_serial_fd >= 0) {
        const char q = '?';
        write(g_serial_fd, &q, 1);
    }
#endif
}

String CommManager::getMachineIP() {
    return String();
}

void CommManager::setMessageCallback(MessageCallback callback) {
    messageCallback = callback;
#if defined(__linux__)
    g_message_callback = callback;
#endif
}

void CommManager::clearMessageCallback() {
    messageCallback = nullptr;
#if defined(__linux__)
    g_message_callback = nullptr;
#endif
}

void CommManager::setTerminalCallback(MessageCallback callback) {
    terminalCallback = callback;
#if defined(__linux__)
    g_terminal_callback = callback;
#endif
}

void CommManager::clearTerminalCallback() {
    terminalCallback = nullptr;
#if defined(__linux__)
    g_terminal_callback = nullptr;
#endif
}

void CommManager::setCommandTap(CommandTap tap) {
    commandTap = tap;
}

void CommManager::clearCommandTap() {
    commandTap = nullptr;
}

void CommManager::setEventCallback(EventCallback callback) {
    eventCallback = callback;
}

void CommManager::clearEventCallback() {
    eventCallback = nullptr;
}

void CommManager::emitConnected() {
    if (!eventCallback) {
        return;
    }
    Event event{};
    event.type = EventType::CONNECTED;
    event.message = "Connected";
    eventCallback(event);
}

void CommManager::emitDisconnected(const char *message) {
    if (!eventCallback) {
        return;
    }
    Event event{};
    event.type = EventType::DISCONNECTED;
    event.message = message;
    eventCallback(event);
}

void CommManager::emitConnectionError(const char *message) {
    if (!eventCallback) {
        return;
    }
    Event event{};
    event.type = EventType::CONNECTION_ERROR;
    event.message = message;
    eventCallback(event);
}

void CommManager::emitProbeResult(float x, float y, float z, bool success) {
    if (!eventCallback) {
        return;
    }
    Event event{};
    event.type = EventType::PROBE_RESULT;
    event.probe = {x, y, z, success};
    eventCallback(event);
}

ConnectionType CommManager::getConnectionType() {
    return currentType;
}

void CommManager::resetJogTracking() {
    jog_pending_valid = false;
    jog_pending_x = 0.0f;
    jog_pending_y = 0.0f;
    jog_pending_z = 0.0f;
    jog_pending_command_count = 0;
    jog_last_reported_x = 0.0f;
    jog_last_reported_y = 0.0f;
    jog_last_reported_z = 0.0f;
    jog_last_state = STATE_DISCONNECTED;
}

void CommManager::updateJogTrackingFromStatus(const FluidNCStatus &status) {
    if (!status.is_connected) {
        jog_pending_valid = false;
        jog_last_state = STATE_DISCONNECTED;
        return;
    }

    const bool jog_to_idle = (jog_last_state == STATE_JOG && status.state == STATE_IDLE);
    jog_last_state = status.state;

    if (!jog_pending_valid) {
        jog_last_reported_x = status.wpos_x;
        jog_last_reported_y = status.wpos_y;
        jog_last_reported_z = status.wpos_z;
        jog_pending_x = 0.0f;
        jog_pending_y = 0.0f;
        jog_pending_z = 0.0f;
        jog_pending_valid = true;
        return;
    }

    if (jog_to_idle) {
        jog_last_reported_x = status.wpos_x;
        jog_last_reported_y = status.wpos_y;
        jog_last_reported_z = status.wpos_z;
        jog_pending_x = 0.0f;
        jog_pending_y = 0.0f;
        jog_pending_z = 0.0f;
        jog_pending_command_count = 0;
        return;
    }

    const bool changed = (status.wpos_x != jog_last_reported_x) ||
                         (status.wpos_y != jog_last_reported_y) ||
                         (status.wpos_z != jog_last_reported_z);
    if (!changed) {
        return;
    }

    const float target_x = jog_last_reported_x + jog_pending_x;
    const float target_y = jog_last_reported_y + jog_pending_y;
    const float target_z = jog_last_reported_z + jog_pending_z;

    jog_last_reported_x = status.wpos_x;
    jog_last_reported_y = status.wpos_y;
    jog_last_reported_z = status.wpos_z;

    jog_pending_x = target_x - jog_last_reported_x;
    jog_pending_y = target_y - jog_last_reported_y;
    jog_pending_z = target_z - jog_last_reported_z;
    if (jog_pending_command_count > 0) {
        jog_pending_command_count--;
    }
}

void CommManager::getPredictedWpos(const FluidNCStatus &status, float &x, float &y, float &z) {
    updateJogTrackingFromStatus(status);
    if (!jog_pending_valid) {
        x = status.wpos_x;
        y = status.wpos_y;
        z = status.wpos_z;
        return;
    }
    x = jog_last_reported_x + jog_pending_x;
    y = jog_last_reported_y + jog_pending_y;
    z = jog_last_reported_z + jog_pending_z;
}

float CommManager::clampJogTarget(float current, float target, bool enabled, float min_limit, float max_limit) {
    if (!enabled) {
        return target;
    }
    if (min_limit > max_limit) {
        float tmp = min_limit;
        min_limit = max_limit;
        max_limit = tmp;
    }

    if (current < min_limit) {
        if (target <= current) {
            return current;
        }
        if (target > max_limit) {
            return max_limit;
        }
        return target;
    }

    if (current > max_limit) {
        if (target >= current) {
            return current;
        }
        if (target < min_limit) {
            return min_limit;
        }
        return target;
    }

    if (target < min_limit) return min_limit;
    if (target > max_limit) return max_limit;
    return target;
}
