#include "core/power_manager.h"

DisplayDriver *PowerManager::display_driver = nullptr;
bool PowerManager::enabled = false;
uint32_t PowerManager::dim_timeout_sec = 0;
uint32_t PowerManager::sleep_timeout_sec = 0;
uint32_t PowerManager::deep_sleep_timeout_sec = 0;
uint8_t PowerManager::normal_brightness = 100;
uint8_t PowerManager::dim_brightness = 50;
uint32_t PowerManager::last_activity_ms = 0;
PowerManager::PowerState PowerManager::current_state = PowerManager::FULL_BRIGHTNESS;
bool PowerManager::state_changed = false;

void PowerManager::init(DisplayDriver *driver) {
    display_driver = driver;
}

void PowerManager::onUserActivity() {}

void PowerManager::update(int) {}

void PowerManager::loadSettings() {}

void PowerManager::saveSettings() {}

void PowerManager::setEnabled(bool enable) { enabled = enable; }

void PowerManager::setDimTimeout(uint32_t seconds) { dim_timeout_sec = seconds; }

void PowerManager::setSleepTimeout(uint32_t seconds) { sleep_timeout_sec = seconds; }

void PowerManager::setDeepSleepTimeout(uint32_t seconds) { deep_sleep_timeout_sec = seconds; }

void PowerManager::setNormalBrightness(uint8_t brightness) { normal_brightness = brightness; }

void PowerManager::setDimBrightness(uint8_t brightness) { dim_brightness = brightness; }

void PowerManager::applyNormalBrightness() {}

void PowerManager::enterFullBrightness() {}
void PowerManager::enterDimmed() {}
void PowerManager::enterScreenOff() {}
void PowerManager::enterDeepSleep() {}
