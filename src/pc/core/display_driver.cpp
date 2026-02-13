#include "core/display_driver.h"

DisplayDriver::DisplayDriver() : disp(nullptr), disp_draw_buf(nullptr), disp_draw_buf2(nullptr) {}

bool DisplayDriver::init() {
    return true;
}

void DisplayDriver::my_disp_flush(lv_display_t *, const lv_area_t *, uint8_t *) {}

void DisplayDriver::setBacklight(uint8_t) {}

void DisplayDriver::setBacklightOn() {}

void DisplayDriver::setBacklightOff() {}

void DisplayDriver::powerDown() {}
