#include "core/grbl_comm.h"
#include "config.h"
#include "core/comm_manager.h"

HardwareSerial *GrblComm::serialPort = nullptr;
FluidNCStatus GrblComm::currentStatus;
uint32_t GrblComm::lastStatusRequestMs = 0;
uint32_t GrblComm::lastStatusRxMs = 0;
bool GrblComm::initialized = false;
MessageCallback GrblComm::messageCallback = nullptr;
MessageCallback GrblComm::terminalCallback = nullptr;
char GrblComm::lineBuffer[256] = {0};
size_t GrblComm::lineLen = 0;

void GrblComm::init() {
    if (initialized) {
        return;
    }
    serialPort = &Serial1;
    initialized = true;
}

bool GrblComm::connect(const MachineConfig &config) {
    (void)config;
    init();
    if (!serialPort) {
        return false;
    }
    serialPort->begin(GRBL_UART_BAUD, SERIAL_8N1, GRBL_UART_RX_PIN, GRBL_UART_TX_PIN);
    lineLen = 0;
    currentStatus = FluidNCStatus();
    lastStatusRequestMs = 0;
    lastStatusRxMs = 0;
    currentStatus.is_connected = false;
    Serial.println("[GrblComm] UART initialized");
    return true;
}

void GrblComm::disconnect() {
    if (serialPort) {
        serialPort->end();
    }
    currentStatus.is_connected = false;
}

bool GrblComm::isConnected() {
    return currentStatus.is_connected;
}

bool GrblComm::isAutoReporting() {
    return false;
}

const FluidNCStatus& GrblComm::getStatus() {
    return currentStatus;
}

void GrblComm::sendCommand(const char* command) {
    if (!serialPort || !command) {
        return;
    }
    serialPort->write(reinterpret_cast<const uint8_t*>(command), strlen(command));
}

void GrblComm::requestStatusReport() {
    if (!serialPort) {
        return;
    }
    serialPort->write('?');
}

String GrblComm::getMachineIP() {
    return String();
}

void GrblComm::setMessageCallback(MessageCallback callback) {
    messageCallback = callback;
}

void GrblComm::clearMessageCallback() {
    messageCallback = nullptr;
}

void GrblComm::setTerminalCallback(MessageCallback callback) {
    terminalCallback = callback;
}

void GrblComm::clearTerminalCallback() {
    terminalCallback = nullptr;
}

void GrblComm::loop() {
    if (!serialPort) {
        return;
    }

    while (serialPort->available()) {
        int ch = serialPort->read();
        if (ch < 0) {
            break;
        }
        if (ch == '\n' || ch == '\r') {
            if (lineLen > 0) {
                lineBuffer[lineLen] = '\0';
                handleLine(lineBuffer);
                lineLen = 0;
            }
            continue;
        }
        if (lineLen < sizeof(lineBuffer) - 1) {
            lineBuffer[lineLen++] = static_cast<char>(ch);
        }
    }

    const uint32_t now = millis();
    if (now - lastStatusRequestMs >= 250) {
        requestStatusReport();
        lastStatusRequestMs = now;
    }

    if (currentStatus.is_connected && lastStatusRxMs != 0 && (now - lastStatusRxMs) > 1000) {
        currentStatus.is_connected = false;
        currentStatus.state = STATE_DISCONNECTED;
    }
}

void GrblComm::handleLine(const char* line) {
    if (!line || line[0] == '\0') {
        return;
    }

    if (messageCallback) {
        messageCallback(line);
    }

    if (line[0] == '<') {
        parseStatusReport(line);
        return;
    }
    if (strncmp(line, "[GC:", 4) == 0) {
        parseGCodeState(line);
        return;
    }
    if (line[0] == '[') {
        parseRealtimeFeedback(line);
    }

    if (terminalCallback) {
        terminalCallback(line);
    }
}

void GrblComm::parseStatusReport(const char* message) {
    lastStatusRxMs = millis();
    currentStatus.last_update_ms = lastStatusRxMs;

    if (!currentStatus.is_connected) {
        currentStatus.is_connected = true;
        CommManager::emitConnected();
        Serial.println("[GrblComm] ✓ Connection established (status received)");
    }

    MachineState newState = currentStatus.state;
    if (strstr(message, "<Idle")) newState = STATE_IDLE;
    else if (strstr(message, "<Run")) newState = STATE_RUN;
    else if (strstr(message, "<Hold")) newState = STATE_HOLD;
    else if (strstr(message, "<Jog")) newState = STATE_JOG;
    else if (strstr(message, "<Alarm")) newState = STATE_ALARM;
    else if (strstr(message, "<Door")) newState = STATE_DOOR;
    else if (strstr(message, "<Check")) newState = STATE_CHECK;
    else if (strstr(message, "<Home")) newState = STATE_HOME;
    else if (strstr(message, "<Sleep")) newState = STATE_SLEEP;
    currentStatus.state = newState;

    const char* mpos = strstr(message, "MPos:");
    if (mpos) {
        sscanf(mpos + 5, "%f,%f,%f", &currentStatus.mpos_x, &currentStatus.mpos_y, &currentStatus.mpos_z);
    }

    const char* wco = strstr(message, "WCO:");
    if (wco) {
        sscanf(wco + 4, "%f,%f,%f", &currentStatus.wco_x, &currentStatus.wco_y, &currentStatus.wco_z);
    }

    currentStatus.wpos_x = currentStatus.mpos_x - currentStatus.wco_x;
    currentStatus.wpos_y = currentStatus.mpos_y - currentStatus.wco_y;
    currentStatus.wpos_z = currentStatus.mpos_z - currentStatus.wco_z;

    const char* wpos = strstr(message, "WPos:");
    if (wpos) {
        sscanf(wpos + 5, "%f,%f,%f", &currentStatus.wpos_x, &currentStatus.wpos_y, &currentStatus.wpos_z);
    }

    const char* fs = strstr(message, "FS:");
    if (fs) {
        sscanf(fs + 3, "%f,%f", &currentStatus.feed_rate, &currentStatus.spindle_speed);
    }

    const char* ov = strstr(message, "Ov:");
    if (ov) {
        sscanf(ov + 3, "%f,%f,%f", &currentStatus.feed_override, &currentStatus.rapid_override, &currentStatus.spindle_override);
    }

    const char* sd = strstr(message, "SD:");
    if (sd) {
        float percent = 0.0f;
        char filename_buf[128] = {0};
        const char* comma = strchr(sd + 3, ',');
        if (comma) {
            sscanf(sd + 3, "%f", &percent);
            strncpy(filename_buf, comma + 1, sizeof(filename_buf) - 1);
            char* end = strchr(filename_buf, '>');
            if (end) *end = '\0';
            end = strchr(filename_buf, '|');
            if (end) *end = '\0';
            currentStatus.is_sd_printing = true;
            currentStatus.sd_percent = percent;
            strncpy(currentStatus.sd_filename, filename_buf, sizeof(currentStatus.sd_filename) - 1);
            currentStatus.sd_filename[sizeof(currentStatus.sd_filename) - 1] = '\0';
            if (currentStatus.sd_start_time_ms == 0) {
                currentStatus.sd_start_time_ms = millis();
            }
            currentStatus.sd_elapsed_ms = millis() - currentStatus.sd_start_time_ms;
        }
    } else {
        currentStatus.is_sd_printing = false;
        currentStatus.sd_percent = 0;
        currentStatus.sd_start_time_ms = 0;
        currentStatus.sd_elapsed_ms = 0;
        currentStatus.sd_filename[0] = '\0';
    }
}

void GrblComm::parseRealtimeFeedback(const char* message) {
    if (strncmp(message, "[PRB:", 5) == 0) {
        float x, y, z;
        int success;
        if (sscanf(message + 5, "%f,%f,%f:%d", &x, &y, &z, &success) == 4) {
            CommManager::emitProbeResult(x, y, z, success != 0);
        }
    }

    if (strncmp(message, "[MSG:", 5) == 0) {
        const char* end = strchr(message, ']');
        if (end) {
            size_t len = end - (message + 5);
            if (len >= sizeof(currentStatus.last_message)) {
                len = sizeof(currentStatus.last_message) - 1;
            }
            strncpy(currentStatus.last_message, message + 5, len);
            currentStatus.last_message[len] = '\0';
        }
    } else {
        strncpy(currentStatus.last_message, message, sizeof(currentStatus.last_message) - 1);
        currentStatus.last_message[sizeof(currentStatus.last_message) - 1] = '\0';
    }
}

void GrblComm::parseGCodeState(const char* message) {
    const char* ptr = message + 4;

    if (strstr(ptr, "G0 ")) strcpy(currentStatus.modal_motion, "G0");
    else if (strstr(ptr, "G1 ")) strcpy(currentStatus.modal_motion, "G1");
    else if (strstr(ptr, "G2 ")) strcpy(currentStatus.modal_motion, "G2");
    else if (strstr(ptr, "G3 ")) strcpy(currentStatus.modal_motion, "G3");
    else if (strstr(ptr, "G80")) strcpy(currentStatus.modal_motion, "G80");

    if (strstr(ptr, "G54")) strcpy(currentStatus.modal_wcs, "G54");
    else if (strstr(ptr, "G55")) strcpy(currentStatus.modal_wcs, "G55");
    else if (strstr(ptr, "G56")) strcpy(currentStatus.modal_wcs, "G56");
    else if (strstr(ptr, "G57")) strcpy(currentStatus.modal_wcs, "G57");
    else if (strstr(ptr, "G58")) strcpy(currentStatus.modal_wcs, "G58");
    else if (strstr(ptr, "G59")) strcpy(currentStatus.modal_wcs, "G59");

    if (strstr(ptr, "G17")) strcpy(currentStatus.modal_plane, "G17");
    else if (strstr(ptr, "G18")) strcpy(currentStatus.modal_plane, "G18");
    else if (strstr(ptr, "G19")) strcpy(currentStatus.modal_plane, "G19");

    if (strstr(ptr, "G20")) strcpy(currentStatus.modal_units, "G20");
    else if (strstr(ptr, "G21")) strcpy(currentStatus.modal_units, "G21");

    if (strstr(ptr, "G90")) strcpy(currentStatus.modal_distance, "G90");
    else if (strstr(ptr, "G91")) strcpy(currentStatus.modal_distance, "G91");

    if (strstr(ptr, "M3 ")) strcpy(currentStatus.modal_spindle, "M3");
    else if (strstr(ptr, "M4 ")) strcpy(currentStatus.modal_spindle, "M4");
    else if (strstr(ptr, "M5")) strcpy(currentStatus.modal_spindle, "M5");

    if (strstr(ptr, "M7 ")) strcpy(currentStatus.modal_coolant, "M7");
    else if (strstr(ptr, "M8 ")) strcpy(currentStatus.modal_coolant, "M8");
    else if (strstr(ptr, "M9")) strcpy(currentStatus.modal_coolant, "M9");

    const char* tool = strstr(ptr, " T");
    if (tool) {
        int toolNum;
        if (sscanf(tool, " T%d", &toolNum) == 1) {
            snprintf(currentStatus.modal_tool, sizeof(currentStatus.modal_tool), "T%d", toolNum);
        }
    }

    const char* feed = strstr(ptr, " F");
    if (feed) {
        float feedValue;
        if (sscanf(feed, " F%f", &feedValue) == 1) {
            if (currentStatus.feed_rate == 0.0f) {
                currentStatus.feed_rate = feedValue;
            }
        }
    }

    const char* spindle = strstr(ptr, " S");
    if (spindle) {
        float spindleValue;
        if (sscanf(spindle, " S%f", &spindleValue) == 1) {
            if (currentStatus.spindle_speed == 0.0f) {
                currentStatus.spindle_speed = spindleValue;
            }
        }
    }
}
