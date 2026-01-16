#include "core/display_driver.h"
#include <esp_heap_caps.h>
#include <Wire.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "core/power_manager.h"

static void indev_activity_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    PowerManager::onUserActivity();
}

// DisplayDriver constructor
DisplayDriver::DisplayDriver() : disp(nullptr), disp_draw_buf(nullptr), disp_draw_buf2(nullptr) {
}

// Initialize display
bool DisplayDriver::init() {
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .hw_cfg = {
#if CONFIG_BSP_LCD_TYPE_HDMI
#if CONFIG_BSP_LCD_HDMI_800x600_60HZ
            .hdmi_resolution = BSP_HDMI_RES_800x600,
#elif CONFIG_BSP_LCD_HDMI_1280x720_60HZ
            .hdmi_resolution = BSP_HDMI_RES_1280x720,
#elif CONFIG_BSP_LCD_HDMI_1280x800_60HZ
            .hdmi_resolution = BSP_HDMI_RES_1280x800,
#elif CONFIG_BSP_LCD_HDMI_1920x1080_30HZ
            .hdmi_resolution = BSP_HDMI_RES_1920x1080,
#endif
#else
            .hdmi_resolution = BSP_HDMI_RES_NONE,
#endif
            .dsi_bus = {
                .phy_clk_src = static_cast<mipi_dsi_phy_clock_source_t>(0),
                .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
            },
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    lv_indev_t *indev = bsp_display_get_input_dev();
    if (indev) {
        lv_indev_add_event_cb(indev, indev_activity_event_cb, LV_EVENT_PRESSED, nullptr);
        lv_indev_add_event_cb(indev, indev_activity_event_cb, LV_EVENT_PRESSING, nullptr);
        lv_indev_add_event_cb(indev, indev_activity_event_cb, LV_EVENT_RELEASED, nullptr);
    }

    bsp_display_lock(0);
    
    return true;
}

// LVGL flush callback
void DisplayDriver::my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
}

// Backlight control methods
void DisplayDriver::setBacklight(uint8_t brightness_percent) {
    // Clamp to valid percentage range
    if (brightness_percent > 100) brightness_percent = 100;
    
    // Convert percentage (0-100) to hardware value (0-255)
    uint8_t hw_value = (brightness_percent * 255) / 100;
    
    bsp_display_brightness_set(brightness_percent);
    Serial.printf("Backlight set to: %d%% (hw=%d)\n", brightness_percent, hw_value);
}

void DisplayDriver::setBacklightOn() {
    bsp_display_backlight_on();
}

void DisplayDriver::setBacklightOff() {
    bsp_display_backlight_off();
}

void DisplayDriver::powerDown() {
    Serial.println("DisplayDriver: Powering down display for deep sleep...");
    
    // Only turn off backlight - do NOT send GT911 sleep command or reconfigure GPIOs
    // The GT911 touch controller on Basic hardware has no reset pin, so if we put it
    // to sleep (register 0x8040 = 0x05), it cannot be woken after ESP32 deep sleep.
    // Leaving GT911 in normal mode allows it to wake naturally when ESP32 wakes.
    // (Advance hardware works either way because STC8H1K28 handles GT911 reset independently)
    setBacklightOff();
    
    Serial.println("  Display powered down (backlight off only)");
}
