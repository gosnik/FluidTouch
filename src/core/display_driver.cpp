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
    
#ifdef BACKLIGHT_PWM
    // Basic: PWM backlight on GPIO2
    ledcWrite(1, hw_value);
#elif defined(BACKLIGHT_I2C)
    // Advance: I2C backlight controller (STC8H1K28 at address 0x30)
    // Brightness: 0 = brightest, 245 = off
    uint8_t i2c_value = 245 - ((hw_value * 245) / 255);
    Wire.beginTransmission(0x30);
    Wire.write(i2c_value);
    Wire.endTransmission();
#endif
    Serial.printf("Backlight set to: %d%% (hw=%d)\n", brightness_percent, hw_value);
}

void DisplayDriver::setBacklightOn() {
    setBacklight(255);
}

void DisplayDriver::setBacklightOff() {
#ifdef BACKLIGHT_PWM
    // Basic: PWM backlight on GPIO2
    ledcWrite(1, 0);
    Serial.println("Backlight OFF (PWM)");
#elif defined(BACKLIGHT_I2C)
    // Advance: I2C backlight controller (STC8H1K28 at address 0x30)
    // Send 0xF5 (245) for off
    Wire.beginTransmission(0x30);
    Wire.write(0xF5);
    Wire.endTransmission();
    Serial.println("Backlight OFF (I2C)");
#endif
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
