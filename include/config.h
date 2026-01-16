#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version
#define FLUIDTOUCH_VERSION "1.0.1"

// Display settings
#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 600

// UI scaling helpers (base layout was 800x480)
#define UI_BASE_WIDTH  800
#define UI_BASE_HEIGHT 480
#define UI_SCALE_X(px) ((px) * SCREEN_WIDTH / UI_BASE_WIDTH)
#define UI_SCALE_Y(px) ((px) * SCREEN_HEIGHT / UI_BASE_HEIGHT)

// Hardware-specific pin configurations
#ifdef HARDWARE_ADVANCE
// CrowPanel 7" Advance - per Elecrow example code
// https://www.elecrow.com/pub/wiki/ESP32_Display-7.0_inch%28Advance_Series%29wiki.html
#define TOUCH_SDA  15
#define TOUCH_SCL  16
#define TOUCH_RST  -1  // Reset handled by STC8H1K28 microcontroller via I2C
#define TOUCH_INT  -1  // Not used
#else
// CrowPanel 7" Basic
#define TOUCH_SDA  19
#define TOUCH_SCL  20
#define TOUCH_RST  38
#define TOUCH_INT  -1
#endif

// Touch controller I2C address
#define GT911_ADDR 0x5D

// GT911 register addresses
#define GT911_POINT_INFO  0x814E
#define GT911_POINT_1     0x814F
#define GT911_CONFIG_REG  0x8047
#define GT911_PRODUCT_ID  0x8140

// UI Layout constants
#define STATUS_BAR_HEIGHT UI_SCALE_Y(60)
#define TAB_BUTTON_HEIGHT UI_SCALE_Y(60)

// Display buffer configuration
#define BUFFER_LINES SCREEN_HEIGHT  // Full screen buffer for smooth rendering (with 8MB PSRAM available)

// Timing constants
#define SPLASH_DURATION_MS 2500

// Preferences namespaces
#define PREFS_NAMESPACE "fluidtouch"        // Machine configurations
#define PREFS_SYSTEM_NAMESPACE "ft_system"  // System flags (clean_shutdown, etc.)

// Grbl UART configuration
#define GRBL_UART_RX_PIN 7
#define GRBL_UART_TX_PIN 8
#define GRBL_UART_BAUD 115200

// Encoder pin configuration
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define ENCODER1_A_PIN 21
#define ENCODER1_B_PIN 22
#define ENCODER2_A_PIN 24
#define ENCODER2_B_PIN 25
#define ENCODER3_A_PIN 28
#define ENCODER3_B_PIN 29
#else
#define ENCODER1_A_PIN -1
#define ENCODER1_B_PIN -1
#define ENCODER2_A_PIN -1
#define ENCODER2_B_PIN -1
#define ENCODER3_A_PIN -1
#define ENCODER3_B_PIN -1
#endif

// WiFi feature toggle (override in build flags)
#ifndef FT_WIFI_ENABLED
#define FT_WIFI_ENABLED true
#endif

// Screenshot server configuration (override in build flags)
#ifndef ENABLE_SCREENSHOT_SERVER
#define ENABLE_SCREENSHOT_SERVER true
#endif

// SD Card Configuration
#ifdef HARDWARE_ADVANCE
// Advance: SPI mode SD card
#define SD_MOSI  6
#define SD_MISO  4
#define SD_CLK   5
#define SD_CS    0  // Not actually connected - CS tied to GND in hardware (per Elecrow example)
#else
// Basic: SPI mode SD card
#define SD_MOSI  11
#define SD_MISO  13
#define SD_CLK   12
#define SD_CS    10
#endif

// Upload Configuration
#define FLUIDNC_UPLOAD_PATH "/fluidtouch/uploads/"

#endif // CONFIG_H
