#include "core/usb_host_manager.h"
#include "core/qtdial_hid_protocol.h"
#include <Arduino.h>

#include <cstring>
#include <string>

#if defined(__linux__)
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <dirent.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
#endif

namespace {
#if defined(__linux__)
constexpr int kMaxQtdialDevices = 3;
constexpr uint32_t kScanIntervalMs = 1000;

struct QtdialDevice {
    bool in_use = false;
    int fd = -1;
    std::string path;
    uint16_t vid = 0;
    uint16_t pid = 0;
    char serial[128] = {0};
    int32_t count = 0;
    struct {
        uint8_t protocol_version = 0;
        uint8_t flags = 0;
        uint8_t buttons = 0;
        int16_t delta = 0;
        int16_t rate = 0;
        uint32_t uptime_ms = 0;
        uint8_t role_id = 0;
        uint16_t seq = 0;
    } last_input;
};

struct UsbHostState {
    bool initialized = false;
    uint32_t last_scan_ms = 0;
    std::string preferred_screen;
    bool preferred_clear = false;
    QtdialDevice devices[kMaxQtdialDevices];
};

UsbHostState g_state;

constexpr uint8_t kDefaultRoleIds[kMaxQtdialDevices] = {
    QtdialHidProtocol::kRoleX,
    QtdialHidProtocol::kRoleY,
    QtdialHidProtocol::kRoleZ,
};

int find_device_slot_by_path(const std::string &path)
{
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (g_state.devices[i].in_use && g_state.devices[i].path == path) {
            return i;
        }
    }
    return -1;
}

int allocate_device_slot()
{
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            g_state.devices[i] = QtdialDevice{};
            g_state.devices[i].in_use = true;
            return i;
        }
    }
    return -1;
}

void close_device_slot(int slot)
{
    if (slot < 0 || slot >= kMaxQtdialDevices) {
        return;
    }
    if (!g_state.devices[slot].in_use) {
        return;
    }
    if (g_state.devices[slot].fd >= 0) {
        close(g_state.devices[slot].fd);
    }
    g_state.devices[slot] = QtdialDevice{};
}

bool is_qtdial_device(uint16_t vid, uint16_t pid)
{
    if (QtdialHidProtocol::kUsbVendorId == 0 || QtdialHidProtocol::kUsbProductId == 0) {
        return true;
    }
    return vid == QtdialHidProtocol::kUsbVendorId && pid == QtdialHidProtocol::kUsbProductId;
}

bool read_hidraw_serial(int fd, char *dst, size_t dst_len)
{
    if (fd < 0 || !dst || dst_len == 0) {
        return false;
    }
    dst[0] = '\0';
    int rc = ioctl(fd, HIDIOCGRAWUNIQ(static_cast<int>(dst_len)), dst);
    if (rc < 0) {
        dst[0] = '\0';
        return false;
    }
    dst[dst_len - 1] = '\0';
    return dst[0] != '\0';
}

bool device_exists(const std::string &path)
{
    return access(path.c_str(), F_OK) == 0;
}

bool send_feature_config(uint8_t slot, uint8_t role_id)
{
    uint8_t payload[4] = {};
    payload[0] = role_id;
    payload[1] = 5;
    payload[2] = 1;
    payload[3] = 0;
    return UsbHostManager::sendOutputReport(slot,
                                            QtdialHidProtocol::kAppReportFeatureConfig,
                                            payload,
                                            sizeof(payload));
}

void parse_input_report(int slot, const uint8_t *data, size_t len)
{
    if (slot < 0 || slot >= kMaxQtdialDevices || !data || len < 2) {
        return;
    }

    const uint8_t *payload = data;
    size_t payload_len = len;

    // Linux hidraw framing varies by kernel/device:
    // it may include a leading 0x00 and/or report ID before payload.
    while (payload_len > 0 && payload[0] == 0) {
        payload++;
        payload_len--;
    }
    if (payload_len > 0 && payload[0] == QtdialHidProtocol::kHidReportId) {
        payload++;
        payload_len--;
    }
    while (payload_len > 0 && payload[0] == 0) {
        payload++;
        payload_len--;
    }

    // If an extra prefix byte remains, allow one-byte shift to recover.
    if (payload_len >= 3 &&
        payload[0] != QtdialHidProtocol::kAppReportInputStatus &&
        payload[1] == QtdialHidProtocol::kAppReportInputStatus &&
        payload[2] == QtdialHidProtocol::kProtocolVersion) {
        payload++;
        payload_len--;
    }
    if (payload_len >= 3 &&
        payload[0] == QtdialHidProtocol::kAppReportInputStatus &&
        payload[1] == QtdialHidProtocol::kAppReportInputStatus &&
        payload[2] == QtdialHidProtocol::kProtocolVersion) {
        payload++;
        payload_len--;
    }

    if (payload_len < 16) {
        return;
    }
    if (payload[0] != QtdialHidProtocol::kAppReportInputStatus ||
        payload[1] != QtdialHidProtocol::kProtocolVersion) {
        return;
    }

    QtdialDevice &dev = g_state.devices[slot];
    dev.last_input.protocol_version = payload[1];
    dev.last_input.flags = payload[2];
    dev.last_input.buttons = payload[3];
    dev.last_input.delta = static_cast<int16_t>(payload[4] | (payload[5] << 8));
    dev.last_input.rate = static_cast<int16_t>(payload[6] | (payload[7] << 8));
    dev.last_input.uptime_ms = static_cast<uint32_t>(payload[8]) |
                               (static_cast<uint32_t>(payload[9]) << 8) |
                               (static_cast<uint32_t>(payload[10]) << 16) |
                               (static_cast<uint32_t>(payload[11]) << 24);
    dev.last_input.role_id = payload[12];
    dev.last_input.seq = static_cast<uint16_t>(payload[14] | (payload[15] << 8));

    if (((dev.last_input.flags & QtdialHidProtocol::kInputStatusFlagHasDelta) != 0) || dev.last_input.delta != 0) {
        dev.count += dev.last_input.delta;
    }
}

void read_input_reports(int slot)
{
    if (slot < 0 || slot >= kMaxQtdialDevices || !g_state.devices[slot].in_use) {
        return;
    }

    uint8_t report[QtdialHidProtocol::kReportSizeBytes + 1] = {};
    while (true) {
        ssize_t rc = read(g_state.devices[slot].fd, report, sizeof(report));
        if (rc > 0) {
            parse_input_report(slot, report, static_cast<size_t>(rc));
            continue;
        }
        if (rc == 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        close_device_slot(slot);
        break;
    }
}

void connect_device_path(const std::string &path)
{
    if (find_device_slot_by_path(path) >= 0) {
        return;
    }

    int fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return;
    }

    hidraw_devinfo info{};
    if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0) {
        close(fd);
        return;
    }

    if (!is_qtdial_device(info.vendor, info.product)) {
        close(fd);
        return;
    }

    int slot = allocate_device_slot();
    if (slot < 0) {
        close(fd);
        return;
    }

    g_state.devices[slot].fd = fd;
    g_state.devices[slot].path = path;
    g_state.devices[slot].vid = info.vendor;
    g_state.devices[slot].pid = info.product;
    read_hidraw_serial(fd, g_state.devices[slot].serial, sizeof(g_state.devices[slot].serial));

    const uint8_t role_id = kDefaultRoleIds[slot];
    g_state.devices[slot].last_input.role_id = role_id;
    send_feature_config(static_cast<uint8_t>(slot), role_id);

    if (!g_state.preferred_screen.empty()) {
        UsbHostManager::sendDisplaySelectScreen(static_cast<uint8_t>(slot),
                                                g_state.preferred_screen.c_str(),
                                                g_state.preferred_clear);
    }
}

void scan_devices()
{
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (g_state.devices[i].in_use && !device_exists(g_state.devices[i].path)) {
            close_device_slot(i);
        }
    }

    DIR *dir = opendir("/dev");
    if (!dir) {
        return;
    }

    std::vector<std::string> paths;
    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "hidraw", 6) != 0) {
            continue;
        }
        paths.emplace_back(std::string("/dev/") + entry->d_name);
    }
    closedir(dir);

    std::sort(paths.begin(), paths.end());
    for (const std::string &path : paths) {
        connect_device_path(path);
    }
}
#endif
} // namespace

void UsbHostManager::init()
{
#if defined(__linux__)
    if (g_state.initialized) {
        return;
    }
    g_state.initialized = true;
    g_state.last_scan_ms = 0;
    scan_devices();
#endif
}

void UsbHostManager::poll()
{
#if defined(__linux__)
    if (!g_state.initialized) {
        return;
    }

    const uint32_t now = millis();
    if ((now - g_state.last_scan_ms) >= kScanIntervalMs) {
        g_state.last_scan_ms = now;
        scan_devices();
    }

    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            continue;
        }
        read_input_reports(i);
    }
#endif
}

bool UsbHostManager::sendOutputReport(uint8_t slot, uint8_t app_report_id, const uint8_t *payload, size_t length)
{
#if defined(__linux__)
    if (!g_state.initialized || slot >= kMaxQtdialDevices) {
        return false;
    }
    if (!g_state.devices[slot].in_use || g_state.devices[slot].fd < 0) {
        return false;
    }
    if (length > (QtdialHidProtocol::kReportSizeBytes - 2)) {
        return false;
    }

    uint8_t report[QtdialHidProtocol::kReportSizeBytes + 1] = {};
    report[0] = QtdialHidProtocol::kHidReportId;
    report[1] = app_report_id;
    report[2] = QtdialHidProtocol::kProtocolVersion;
    if (payload && length > 0) {
        memcpy(&report[3], payload, length);
    }

    ssize_t rc = write(g_state.devices[slot].fd, report, sizeof(report));
    if (rc != static_cast<ssize_t>(sizeof(report))) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close_device_slot(static_cast<int>(slot));
        }
        return false;
    }
    return true;
#else
    (void)slot;
    (void)app_report_id;
    (void)payload;
    (void)length;
    return false;
#endif
}

bool UsbHostManager::sendDisplaySelectScreen(uint8_t slot, const char *screen_name, bool clear_before_load)
{
    if (!screen_name || screen_name[0] == '\0') {
        return false;
    }
    const size_t max_payload = QtdialHidProtocol::kReportSizeBytes - 2;
    const size_t max_key = (max_payload > 4) ? (max_payload - 4) : 0;
    const size_t key_len = strnlen(screen_name, max_key);
    if (key_len == 0 || (4 + key_len) > max_payload) {
        return false;
    }
    uint8_t payload[QtdialHidProtocol::kReportSizeBytes] = {};
    payload[0] = QtdialHidProtocol::kDisplayCmdSelectScreen;
    payload[1] = clear_before_load ? QtdialHidProtocol::kDisplayFlagClearBeforeLoad : 0;
    payload[2] = static_cast<uint8_t>(key_len);
    payload[3] = 0;
    memcpy(&payload[4], screen_name, key_len);
    return sendOutputReport(slot, QtdialHidProtocol::kAppReportOutputDisplay, payload, 4 + key_len);
}

bool UsbHostManager::sendDisplaySelectScreenAll(const char *screen_name, bool clear_before_load)
{
#if defined(__linux__)
    bool sent = false;
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            continue;
        }
        if (sendDisplaySelectScreen(static_cast<uint8_t>(i), screen_name, clear_before_load)) {
            sent = true;
        }
    }
    return sent;
#else
    (void)screen_name;
    (void)clear_before_load;
    return false;
#endif
}

bool UsbHostManager::sendDisplaySelectScreenForRole(uint8_t role_id, const char *screen_name, bool clear_before_load)
{
#if defined(__linux__)
    if (screen_name && screen_name[0] != '\0') {
        g_state.preferred_screen = screen_name;
        g_state.preferred_clear = clear_before_load;
    }
    bool sent = false;
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            continue;
        }
        if (g_state.devices[i].last_input.role_id != role_id) {
            continue;
        }
        if (sendDisplaySelectScreen(static_cast<uint8_t>(i), screen_name, clear_before_load)) {
            sent = true;
        }
    }
    return sent;
#else
    (void)role_id;
    (void)screen_name;
    (void)clear_before_load;
    return false;
#endif
}

bool UsbHostManager::setPreferredScreen(const char *screen_name, bool clear_before_load)
{
#if defined(__linux__)
    if (!screen_name || screen_name[0] == '\0') {
        g_state.preferred_screen.clear();
        g_state.preferred_clear = false;
        return false;
    }
    g_state.preferred_screen = screen_name;
    g_state.preferred_clear = clear_before_load;
    return true;
#else
    (void)screen_name;
    (void)clear_before_load;
    return false;
#endif
}

bool UsbHostManager::sendDisplaySetField(uint8_t slot, const char *field_name, const char *value)
{
    if (!field_name || field_name[0] == '\0' || !value) {
        return false;
    }
    const size_t max_payload = QtdialHidProtocol::kReportSizeBytes - 2;
    const size_t max_data = (max_payload > 4) ? (max_payload - 4) : 0;
    const size_t key_len = strnlen(field_name, max_data);
    const size_t remaining = (key_len < max_data) ? (max_data - key_len) : 0;
    const size_t val_len = strnlen(value, remaining);
    if (key_len == 0 || (4 + key_len + val_len) > max_payload) {
        return false;
    }
    uint8_t payload[QtdialHidProtocol::kReportSizeBytes] = {};
    payload[0] = QtdialHidProtocol::kDisplayCmdSetField;
    payload[1] = 0;
    payload[2] = static_cast<uint8_t>(key_len);
    payload[3] = static_cast<uint8_t>(val_len);
    memcpy(&payload[4], field_name, key_len);
    memcpy(&payload[4 + key_len], value, val_len);
    return sendOutputReport(slot, QtdialHidProtocol::kAppReportOutputDisplay, payload, 4 + key_len + val_len);
}

bool UsbHostManager::sendDisplaySetFieldAll(const char *field_name, const char *value)
{
#if defined(__linux__)
    bool sent = false;
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            continue;
        }
        if (sendDisplaySetField(static_cast<uint8_t>(i), field_name, value)) {
            sent = true;
        }
    }
    return sent;
#else
    (void)field_name;
    (void)value;
    return false;
#endif
}

bool UsbHostManager::sendDisplayClearFields(uint8_t slot)
{
    uint8_t payload[4] = {};
    payload[0] = QtdialHidProtocol::kDisplayCmdClearFields;
    payload[1] = 0;
    payload[2] = 0;
    payload[3] = 0;
    return sendOutputReport(slot, QtdialHidProtocol::kAppReportOutputDisplay, payload, sizeof(payload));
}

bool UsbHostManager::sendDisplayBrightness(uint8_t slot, uint8_t brightness)
{
    uint8_t payload[5] = {};
    payload[0] = QtdialHidProtocol::kDisplayCmdSetBrightness;
    payload[1] = 0;
    payload[2] = 0;
    payload[3] = 1;
    payload[4] = brightness;
    return sendOutputReport(slot, QtdialHidProtocol::kAppReportOutputDisplay, payload, sizeof(payload));
}

bool UsbHostManager::sendDisplayBrightnessAll(uint8_t brightness)
{
#if defined(__linux__)
    bool sent = false;
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            continue;
        }
        if (sendDisplayBrightness(static_cast<uint8_t>(i), brightness)) {
            sent = true;
        }
    }
    return sent;
#else
    (void)brightness;
    return false;
#endif
}

bool UsbHostManager::sendStatusAll(const uint8_t *payload, size_t length)
{
#if defined(__linux__)
    bool sent = false;
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            continue;
        }
        if (sendOutputReport(static_cast<uint8_t>(i), QtdialHidProtocol::kAppReportOutputStatus, payload, length)) {
            sent = true;
        }
    }
    return sent;
#else
    (void)payload;
    (void)length;
    return false;
#endif
}

bool UsbHostManager::getRoleCount(uint8_t role_id, int32_t *count_out)
{
#if defined(__linux__)
    if (!count_out) {
        return false;
    }
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            continue;
        }
        if (g_state.devices[i].last_input.role_id == role_id) {
            *count_out = g_state.devices[i].count;
            return true;
        }
    }
    return false;
#else
    (void)role_id;
    (void)count_out;
    return false;
#endif
}

bool UsbHostManager::getSingleDeviceCount(int32_t *count_out)
{
#if defined(__linux__)
    if (!count_out) {
        return false;
    }
    int slot = -1;
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            continue;
        }
        if (slot >= 0) {
            return false;
        }
        slot = i;
    }
    if (slot < 0) {
        return false;
    }
    *count_out = g_state.devices[slot].count;
    return true;
#else
    (void)count_out;
    return false;
#endif
}

int UsbHostManager::deviceCount()
{
#if defined(__linux__)
    int count = 0;
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (g_state.devices[i].in_use) {
            ++count;
        }
    }
    return count;
#else
    return 0;
#endif
}
