#include "core/usb_host_manager.h"

void UsbHostManager::init() {}
void UsbHostManager::poll() {}

bool UsbHostManager::sendOutputReport(uint8_t, uint8_t, const uint8_t *, size_t) { return false; }
bool UsbHostManager::sendDisplaySelectScreen(uint8_t, const char *, bool) { return false; }
bool UsbHostManager::sendDisplaySelectScreenAll(const char *, bool) { return false; }
bool UsbHostManager::sendDisplaySelectScreenForRole(uint8_t, const char *, bool) { return false; }
bool UsbHostManager::setPreferredScreen(const char *, bool) { return false; }
bool UsbHostManager::sendDisplaySetField(uint8_t, const char *, const char *) { return false; }
bool UsbHostManager::sendDisplaySetFieldAll(const char *, const char *) { return false; }
bool UsbHostManager::sendDisplayClearFields(uint8_t) { return false; }
bool UsbHostManager::sendDisplayBrightness(uint8_t, uint8_t) { return false; }
bool UsbHostManager::sendDisplayBrightnessAll(uint8_t) { return false; }
bool UsbHostManager::sendStatusAll(const uint8_t *, size_t) { return false; }
bool UsbHostManager::getRoleCount(uint8_t, int32_t *count_out) {
    if (count_out) {
        *count_out = 0;
    }
    return false;
}
bool UsbHostManager::getSingleDeviceCount(int32_t *count_out) {
    if (count_out) {
        *count_out = 0;
    }
    return false;
}
int UsbHostManager::deviceCount() { return 0; }
