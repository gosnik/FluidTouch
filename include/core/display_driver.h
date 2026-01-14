#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <lvgl.h>
#include "config.h"

// Display driver class
class DisplayDriver {
public:
    DisplayDriver();
    bool init();
    lv_display_t* getDisplay() { return disp; }
    
    // Direct screen buffer access for screenshots
    //LGFX* getLCD() { return &lcd; }
    
    // Backlight control (brightness 0-100 percentage, or use setBacklightOn/Off for simple on/off)
    void setBacklight(uint8_t brightness_percent);
    void setBacklightOn();
    void setBacklightOff();
    
    // Power management - deep sleep preparation
    void powerDown();
    
private:
    //LGFX lcd;
    lv_display_t *disp;
    lv_color_t *disp_draw_buf;
    lv_color_t *disp_draw_buf2;
    
    static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
};

#endif // DISPLAY_DRIVER_H
