#ifndef USB_HOST_MANAGER_H
#define USB_HOST_MANAGER_H

#include <cstddef>
#include <cstdint>

class UsbHostManager {
public:
    static void init();
    static void poll();
    static bool sendOutputReport(uint8_t slot, uint8_t app_report_id, const uint8_t *payload, size_t length);
    static bool sendDisplaySelectScreen(uint8_t slot, const char *screen_name, bool clear_before_load);
    static bool sendDisplaySelectScreenAll(const char *screen_name, bool clear_before_load);
    static bool sendDisplaySelectScreenForRole(uint8_t role_id, const char *screen_name, bool clear_before_load);
    static bool setPreferredScreen(const char *screen_name, bool clear_before_load);
    static bool sendDisplaySetField(uint8_t slot, const char *field_name, const char *value);
    static bool sendDisplaySetFieldAll(const char *field_name, const char *value);
    static bool sendDisplayClearFields(uint8_t slot);
    static bool sendDisplayBrightness(uint8_t slot, uint8_t brightness);
    static bool sendDisplayBrightnessAll(uint8_t brightness);
    static bool sendStatusAll(const uint8_t *payload, size_t length);
    static bool getRoleCount(uint8_t role_id, int32_t *count_out);
    static int deviceCount();
};

#endif // USB_HOST_MANAGER_H
