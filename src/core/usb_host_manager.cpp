#include "core/usb_host_manager.h"
#include "core/qtdial_hid_protocol.h"
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include <cstring>
#include <string>
#include <Preferences.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "bsp/esp32_p4_function_ev_board.h"
#include "usb/hid_host.h"
#endif

namespace {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
constexpr int kMaxQtdialDevices = 3;
constexpr size_t kMaxReportSize = 256;
constexpr int kEventQueueDepth = 16;

const char *kTag = "UsbHostManager";

enum class HidEventType : uint8_t {
    Connected,
    Disconnected,
    InputReport,
    TransferError,
};

struct HidEvent {
    HidEventType type;
    hid_host_device_handle_t handle;
    size_t report_len;
    uint8_t report[kMaxReportSize];
};

struct QtdialDevice {
    bool in_use = false;
    hid_host_device_handle_t handle = nullptr;
    hid_host_dev_params_t params{};
    hid_host_dev_info_t info{};
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
    QueueHandle_t event_queue = nullptr;
    QtdialDevice devices[kMaxQtdialDevices];
    const char *preferred_screen = nullptr;
    bool preferred_clear = false;
};

UsbHostState g_state;

constexpr uint8_t kDefaultRoleIds[kMaxQtdialDevices] = {
    QtdialHidProtocol::kRoleX,
    QtdialHidProtocol::kRoleY,
    QtdialHidProtocol::kRoleZ,
};

constexpr const char *kPrefsNamespace = "qtdial";
constexpr const char *kPrefsKeySerialX = "serial_x";
constexpr const char *kPrefsKeySerialY = "serial_y";
constexpr const char *kPrefsKeySerialZ = "serial_z";
constexpr const char *kDefaultSerialX = "QTDIAL-X-0001";
constexpr const char *kDefaultSerialY = "QTDIAL-Y-0001";
constexpr const char *kDefaultSerialZ = "QTDIAL-Z-0001";

String g_role_serials[kMaxQtdialDevices];
bool g_role_serials_loaded = false;

static void load_role_serials()
{
    if (g_role_serials_loaded) {
        return;
    }
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, false)) {
        return;
    }
    auto get_or_set = [&](const char *key, const char *def) -> String {
        String val = prefs.getString(key, "");
        if (val.length() == 0) {
            prefs.putString(key, def);
            val = def;
        }
        return val;
    };
    g_role_serials[0] = get_or_set(kPrefsKeySerialX, kDefaultSerialX);
    g_role_serials[1] = get_or_set(kPrefsKeySerialY, kDefaultSerialY);
    g_role_serials[2] = get_or_set(kPrefsKeySerialZ, kDefaultSerialZ);
    prefs.end();
    g_role_serials_loaded = true;
}

static bool role_from_serial(const char *serial, uint8_t *role_out)
{
    if (!serial || !role_out) {
        return false;
    }
    if (!g_role_serials_loaded) {
        load_role_serials();
    }
    if (g_role_serials[0].length() && g_role_serials[0].equals(serial)) {
        *role_out = QtdialHidProtocol::kRoleX;
        return true;
    }
    if (g_role_serials[1].length() && g_role_serials[1].equals(serial)) {
        *role_out = QtdialHidProtocol::kRoleY;
        return true;
    }
    if (g_role_serials[2].length() && g_role_serials[2].equals(serial)) {
        *role_out = QtdialHidProtocol::kRoleZ;
        return true;
    }
    return false;
}

static bool send_feature_config(uint8_t slot, uint8_t role_id)
{
    uint8_t payload[4] = {};
    payload[0] = role_id;
    payload[1] = 5;  // sensitivity default
    payload[2] = 1;  // detent_div default
    payload[3] = 0;  // report_interval_10ms (0 = event-driven)
    return UsbHostManager::sendOutputReport(slot, QtdialHidProtocol::kAppReportFeatureConfig, payload, sizeof(payload));
}

static void wchar_to_ascii(const wchar_t *src, char *dst, size_t dst_len)
{
    if (!src || !dst || dst_len == 0) {
        return;
    }
    size_t i = 0;
    for (; i + 1 < dst_len && src[i] != 0; ++i) {
        wchar_t ch = src[i];
        dst[i] = (ch < 0x80) ? static_cast<char>(ch) : '?';
    }
    dst[i] = '\0';
}

static int find_device_slot(hid_host_device_handle_t handle)
{
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (g_state.devices[i].in_use && g_state.devices[i].handle == handle) {
            return i;
        }
    }
    return -1;
}

static int allocate_device_slot(hid_host_device_handle_t handle)
{
    for (int i = 0; i < kMaxQtdialDevices; ++i) {
        if (!g_state.devices[i].in_use) {
            g_state.devices[i] = QtdialDevice{};
            g_state.devices[i].in_use = true;
            g_state.devices[i].handle = handle;
            return i;
        }
    }
    return -1;
}

static void release_device_slot(hid_host_device_handle_t handle)
{
    int slot = find_device_slot(handle);
    if (slot < 0) {
        return;
    }
    g_state.devices[slot] = QtdialDevice{};
}

static bool is_qtdial_device(const hid_host_dev_info_t &info)
{
    if (QtdialHidProtocol::kUsbVendorId == 0 || QtdialHidProtocol::kUsbProductId == 0) {
        return true;
    }
    return info.VID == QtdialHidProtocol::kUsbVendorId && info.PID == QtdialHidProtocol::kUsbProductId;
}

static void handle_connected(hid_host_device_handle_t handle)
{
    if (!handle) {
        return;
    }

    if (find_device_slot(handle) >= 0) {
        return;
    }

    int slot = allocate_device_slot(handle);
    if (slot < 0) {
        ESP_LOGW(kTag, "No free qtdial slots; dropping device");
        hid_host_device_close(handle);
        return;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(hid_host_device_get_params(handle, &g_state.devices[slot].params));
    ESP_ERROR_CHECK_WITHOUT_ABORT(hid_host_get_device_info(handle, &g_state.devices[slot].info));

    if (!is_qtdial_device(g_state.devices[slot].info)) {
        ESP_LOGI(kTag, "Ignoring non-qtdial HID device (VID=0x%04x PID=0x%04x)",
                 g_state.devices[slot].info.VID,
                 g_state.devices[slot].info.PID);
        hid_host_device_close(handle);
        release_device_slot(handle);
        return;
    }

    char manufacturer[HID_STR_DESC_MAX_LENGTH * 2] = {};
    char product[HID_STR_DESC_MAX_LENGTH * 2] = {};
    char serial[HID_STR_DESC_MAX_LENGTH * 2] = {};
    wchar_to_ascii(g_state.devices[slot].info.iManufacturer, manufacturer, sizeof(manufacturer));
    wchar_to_ascii(g_state.devices[slot].info.iProduct, product, sizeof(product));
    wchar_to_ascii(g_state.devices[slot].info.iSerialNumber, serial, sizeof(serial));

    ESP_LOGI(kTag, "HID device slot=%d VID=0x%04x PID=0x%04x SN=%s MFG=%s PROD=%s",
             slot,
             g_state.devices[slot].info.VID,
             g_state.devices[slot].info.PID,
             serial,
             manufacturer,
             product);

    ESP_ERROR_CHECK_WITHOUT_ABORT(hid_class_request_set_idle(handle, 0, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_REPORT));
    ESP_ERROR_CHECK_WITHOUT_ABORT(hid_host_device_start(handle));

    // Report descriptor dump disabled for production.

    if (slot < kMaxQtdialDevices) {
        uint8_t role_id = kDefaultRoleIds[slot];
        if (role_from_serial(serial, &role_id)) {
            ESP_LOGI(kTag, "Mapped serial %s to role_id=%u", serial, role_id);
        }
        if (send_feature_config(static_cast<uint8_t>(slot), role_id)) {
            ESP_LOGI(kTag, "Assigned qtdial slot=%d role_id=%u", slot, role_id);
            g_state.devices[slot].last_input.role_id = role_id;
        } else {
            ESP_LOGW(kTag, "Failed to assign role_id for slot=%d", slot);
        }

        if (g_state.preferred_screen && g_state.preferred_screen[0] != '\0') {
            UsbHostManager::sendDisplaySelectScreen(static_cast<uint8_t>(slot),
                                                    g_state.preferred_screen,
                                                    g_state.preferred_clear);
        }
    }
}

static void push_event(const HidEvent &event)
{
    if (!g_state.event_queue) {
        return;
    }
    xQueueSend(g_state.event_queue, &event, 0);
}

static void hid_interface_event_cb(hid_host_device_handle_t hid_dev_handle,
                                   const hid_host_interface_event_t event,
                                   void *arg)
{
    (void)arg;
    HidEvent msg{};
    msg.handle = hid_dev_handle;

    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
            msg.type = HidEventType::InputReport;
            msg.report_len = 0;
            esp_err_t err = hid_host_device_get_raw_input_report_data(hid_dev_handle,
                                                                      msg.report,
                                                                      sizeof(msg.report),
                                                                      &msg.report_len);
            if (err != ESP_OK) {
                ESP_LOGW(kTag, "Failed to read HID report: %s", esp_err_to_name(err));
                return;
            }
            push_event(msg);
            break;
        }
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            msg.type = HidEventType::TransferError;
            push_event(msg);
            break;
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            msg.type = HidEventType::Disconnected;
            push_event(msg);
            break;
        default:
            break;
    }
}

static void hid_driver_event_cb(hid_host_device_handle_t hid_device_handle,
                                const hid_host_driver_event_t event,
                                void *arg)
{
    (void)arg;
    if (event != HID_HOST_DRIVER_EVENT_CONNECTED) {
        return;
    }

    hid_host_device_config_t dev_cfg = {
        .callback = hid_interface_event_cb,
        .callback_arg = nullptr,
    };

    esp_err_t err = hid_host_device_open(hid_device_handle, &dev_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to open HID device: %s", esp_err_to_name(err));
        return;
    }

    HidEvent msg{};
    msg.type = HidEventType::Connected;
    msg.handle = hid_device_handle;
    push_event(msg);
}

static void handle_event(const HidEvent &event)
{
    switch (event.type) {
        case HidEventType::Connected:
            handle_connected(event.handle);
            break;
        case HidEventType::Disconnected:
            ESP_LOGI(kTag, "HID device disconnected");
            hid_host_device_close(event.handle);
            release_device_slot(event.handle);
            break;
        case HidEventType::TransferError:
            ESP_LOGW(kTag, "HID transfer error");
            break;
        case HidEventType::InputReport: {
            int slot = find_device_slot(event.handle);
            if (slot < 0) {
                break;
            }
            // TODO: Parse qtdial custom HID report payload and route to control logic.
            if (event.report_len < 2) {
                break;
            }
            const uint8_t *payload = event.report;
            size_t payload_len = event.report_len;
            // HID stacks sometimes include a leading report ID byte. Accept both forms.
            if (payload_len >= 3 &&
                payload[0] != QtdialHidProtocol::kAppReportInputStatus &&
                payload[1] == QtdialHidProtocol::kAppReportInputStatus &&
                payload[2] == QtdialHidProtocol::kProtocolVersion) {
                payload++;
                payload_len--;
            } else if (payload_len >= 3 &&
                       payload[0] == QtdialHidProtocol::kAppReportInputStatus &&
                       payload[1] == QtdialHidProtocol::kAppReportInputStatus &&
                       payload[2] == QtdialHidProtocol::kProtocolVersion) {
                payload++;
                payload_len--;
            } else if (payload_len > 0 && payload[0] == 0) {
                payload++;
                payload_len--;
            }
            if (payload_len < 2) {
                break;
            }
            const uint8_t app_id = payload[0];
            const uint8_t version = payload[1];
            if (app_id != QtdialHidProtocol::kAppReportInputStatus) {
                break;
            }
            if (version != QtdialHidProtocol::kProtocolVersion) {
                break;
            }
            if (payload_len < 16) {
                break;
            }

            QtdialDevice &dev = g_state.devices[slot];
            dev.last_input.protocol_version = version;
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
            if ((dev.last_input.flags & 0x02) != 0) {
                dev.count += dev.last_input.delta;
            }

            ESP_LOGI(kTag,
                     "qtdial slot=%d delta=%d rate=%d flags=0x%02x buttons=0x%02x role=%u",
                     slot,
                     dev.last_input.delta,
                     dev.last_input.rate,
                     dev.last_input.flags,
                     dev.last_input.buttons,
                     dev.last_input.role_id);
            break;
        }
        default:
            break;
    }
}
#endif
} // namespace

void UsbHostManager::init()
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (g_state.initialized) {
        return;
    }

    ESP_LOGI(kTag, "UsbHostManager::init enter");
    load_role_serials();

    ESP_LOGI(kTag, "Starting USB host...");
    esp_err_t err = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, false);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "USB host start failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(kTag, "USB host started");

    const hid_host_driver_config_t hid_cfg = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = tskNO_AFFINITY,
        .callback = hid_driver_event_cb,
        .callback_arg = nullptr,
    };

    err = hid_host_install(&hid_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "HID host install failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(kTag, "HID host installed");

    g_state.event_queue = xQueueCreate(kEventQueueDepth, sizeof(HidEvent));
    if (!g_state.event_queue) {
        ESP_LOGE(kTag, "Failed to create HID event queue");
        return;
    }
    ESP_LOGI(kTag, "HID event queue created");

    g_state.initialized = true;
#else
    ESP_LOGI("UsbHostManager", "CONFIG_IDF_TARGET_ESP32P4 not defined in usb_host_manager.cpp");
#endif
}

void UsbHostManager::poll()
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (!g_state.initialized || !g_state.event_queue) {
        return;
    }

    HidEvent event{};
    while (xQueueReceive(g_state.event_queue, &event, 0) == pdTRUE) {
        handle_event(event);
    }
#endif
}

bool UsbHostManager::sendOutputReport(uint8_t slot, uint8_t app_report_id, const uint8_t *payload, size_t length)
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (!g_state.initialized || slot >= kMaxQtdialDevices) {
        return false;
    }
    if (!g_state.devices[slot].in_use) {
        return false;
    }
    if (length > (QtdialHidProtocol::kReportSizeBytes - 2)) {
        return false;
    }
    uint8_t report[QtdialHidProtocol::kReportSizeBytes] = {};
    report[0] = app_report_id;
    report[1] = QtdialHidProtocol::kProtocolVersion;
    if (payload && length > 0) {
        memcpy(&report[2], payload, length);
    }

    //ESP_LOGI(kTag, "sendOutputReport: slot=%u app_id=0x%02X len=%u",
    //         slot,
    //         app_report_id,
    //         static_cast<unsigned>(length));
    esp_err_t err = hid_class_request_set_report(g_state.devices[slot].handle,
                                                 HID_REPORT_TYPE_OUTPUT,
                                                 QtdialHidProtocol::kHidReportId,
                                                 report,
                                                 sizeof(report));
    //ESP_LOGI(kTag, "sendOutputReport: slot=%u app_id=0x%02X err=%s",
    //         slot,
    //         app_report_id,
    //         esp_err_to_name(err));
    if (app_report_id == QtdialHidProtocol::kAppReportOutputDisplay) {
        ESP_LOGI(kTag, "Display report send: slot=%u payload_len=%u err=%s",
                 slot,
                 static_cast<unsigned>(length),
                 esp_err_to_name(err));
    }
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to send output report: %s", esp_err_to_name(err));
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
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (!screen_name || screen_name[0] == '\0') {
        return false;
    }
    ESP_LOGI(kTag, "Display select: slot=%u screen=%s clear=%d",
             slot,
             screen_name,
             clear_before_load ? 1 : 0);
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
#else
    (void)slot;
    (void)screen_name;
    (void)clear_before_load;
    return false;
#endif
}

bool UsbHostManager::sendDisplaySelectScreenAll(const char *screen_name, bool clear_before_load)
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
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
#if defined(CONFIG_IDF_TARGET_ESP32P4)
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
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    if (!screen_name || screen_name[0] == '\0') {
        g_state.preferred_screen = nullptr;
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
#if defined(CONFIG_IDF_TARGET_ESP32P4)
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
#else
    (void)slot;
    (void)field_name;
    (void)value;
    return false;
#endif
}

bool UsbHostManager::sendDisplaySetFieldAll(const char *field_name, const char *value)
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
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
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    uint8_t payload[4] = {};
    payload[0] = QtdialHidProtocol::kDisplayCmdClearFields;
    payload[1] = 0;
    payload[2] = 0;
    payload[3] = 0;
    return sendOutputReport(slot, QtdialHidProtocol::kAppReportOutputDisplay, payload, sizeof(payload));
#else
    (void)slot;
    return false;
#endif
}

bool UsbHostManager::sendDisplayBrightness(uint8_t slot, uint8_t brightness)
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    uint8_t payload[5] = {};
    payload[0] = QtdialHidProtocol::kDisplayCmdSetBrightness;
    payload[1] = 0;
    payload[2] = 0;
    payload[3] = 1;
    payload[4] = brightness;
    return sendOutputReport(slot, QtdialHidProtocol::kAppReportOutputDisplay, payload, sizeof(payload));
#else
    (void)slot;
    (void)brightness;
    return false;
#endif
}

bool UsbHostManager::sendDisplayBrightnessAll(uint8_t brightness)
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
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
#if defined(CONFIG_IDF_TARGET_ESP32P4)
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
#if defined(CONFIG_IDF_TARGET_ESP32P4)
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

int UsbHostManager::deviceCount()
{
#if defined(CONFIG_IDF_TARGET_ESP32P4)
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
