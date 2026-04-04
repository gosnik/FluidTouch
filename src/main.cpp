#include <Arduino.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include <cstring>
#include <lvgl.h>
#include "config.h"
#if FT_WIFI_ENABLED
#include "network/wifi_manager.h"
#endif
#include <Preferences.h>
#include "core/display_driver.h"     // Display driver module
#include "core/encoder.h"
#include "core/power_manager.h"      // Power management module
#include "core/qtdial_button_mapping.h"
#include "core/usb_host_manager.h"
#include "core/qtdial_hid_protocol.h"
#include "network/screenshot_server.h"  // Screenshot web server
#include "core/comm_manager.h"     // Machine comms (WebSocket/UART)
#include "ui/ui_theme.h"        // UI theme colors
#include "ui/ui_splash.h"       // Splash screen module
#include "ui/ui_machine_select.h" // Machine selection screen
#include "ui/ui_common.h"       // UI common components (status bar)
#include "ui/ui_tabs.h"         // UI tabs module
#include "ui/settings_manager.h" // Settings import/export/clear
#include "ui/tabs/ui_tab_status.h" // Status tab for updates
#include "ui/tabs/ui_tab_files.h" // Files tab for refresh check
#include "ui/tabs/ui_tab_macros.h" // Macros tab for progress updates
#include "ui/tabs/ui_tab_terminal.h" // Terminal tab for updates
#include "ui/tabs/settings/ui_tab_settings_about.h" // About tab for screenshot URL updates
#include "ui/tabs/control/ui_tab_control_actions.h" // Actions tab for pause button updates
#include "ui/tabs/control/ui_tab_control_jog.h" // Jog tab for current feed setting
#include "ui/tabs/control/ui_tab_control_override.h" // Override tab for updates
#include "ui/machine_config.h"  // Machine configuration manager

#ifndef TAG
#define TAG "Main"
#endif

namespace {
void sendHidStatus(const FluidNCStatus &status, bool connected)
{
    if (UsbHostManager::deviceCount() <= 0) {
        return;
    }

    uint8_t payload[36] = {};
    payload[0] = static_cast<uint8_t>(connected ? status.state : STATE_DISCONNECTED);
    payload[1] = (UICommon::isEncoderBindEnabled() ? QtdialHidProtocol::kOutputStatusFlagEncoderEnabled : 0x00) |
                 (connected ? (QtdialHidProtocol::kOutputStatusFlagConnected |
                               QtdialHidProtocol::kOutputStatusFlagHasWpos |
                               QtdialHidProtocol::kOutputStatusFlagHasMpos)
                            : 0x00);

    const int32_t jog_xy_feed = UITabControlJog::getCurrentXYFeed();
    const int32_t feed = static_cast<int32_t>(jog_xy_feed * QtdialHidProtocol::kStatusFeedScale);
    const int32_t spindle = static_cast<int32_t>(status.spindle_speed);
    const int32_t wpos_x = static_cast<int32_t>(status.wpos_x * QtdialHidProtocol::kStatusPosScale);
    const int32_t wpos_y = static_cast<int32_t>(status.wpos_y * QtdialHidProtocol::kStatusPosScale);
    const int32_t wpos_z = static_cast<int32_t>(status.wpos_z * QtdialHidProtocol::kStatusPosScale);
    const int32_t mpos_x = static_cast<int32_t>(status.mpos_x * QtdialHidProtocol::kStatusPosScale);
    const int32_t mpos_y = static_cast<int32_t>(status.mpos_y * QtdialHidProtocol::kStatusPosScale);
    const int32_t mpos_z = static_cast<int32_t>(status.mpos_z * QtdialHidProtocol::kStatusPosScale);

    memcpy(&payload[2], &feed, sizeof(feed));
    memcpy(&payload[6], &spindle, sizeof(spindle));
    memcpy(&payload[10], &wpos_x, sizeof(wpos_x));
    memcpy(&payload[14], &wpos_y, sizeof(wpos_y));
    memcpy(&payload[18], &wpos_z, sizeof(wpos_z));
    memcpy(&payload[22], &mpos_x, sizeof(mpos_x));
    memcpy(&payload[26], &mpos_y, sizeof(mpos_y));
    memcpy(&payload[30], &mpos_z, sizeof(mpos_z));

    UsbHostManager::sendStatusAll(payload, 34);
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(1000);
    //esp_log_level_set("*", ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "\n\n=== FluidTouch - LVGL 9 with LovyanGFX ===");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());

    // Initialize Display Driver
    ESP_LOGI(TAG, "Initializing display driver...");
    static DisplayDriver displayDriver;
    if (!displayDriver.init()) {
        ESP_LOGI(TAG, "ERROR: Failed to initialize display!");
        while (1) delay(1000);
    }
    ESP_LOGI(TAG, "Display driver initialized successfully");

    // Initialize Power Manager
    ESP_LOGI(TAG, "Initializing power manager...");
    PowerManager::init(&displayDriver);
    ESP_LOGI(TAG, "Power manager initialized successfully");

    #if FT_WIFI_ENABLED
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    WifiManager::init();
    #endif

    #if defined(CONFIG_IDF_TARGET_ESP32P4)
    ESP_LOGI(TAG, "CONFIG_IDF_TARGET_ESP32P4 defined");
    ESP_LOGI(TAG, "Initializing encoders...");
    const EncoderPins encoder_pins[] = {
        {ENCODER1_A_PIN, ENCODER1_B_PIN},
        {ENCODER2_A_PIN, ENCODER2_B_PIN},
        {ENCODER3_A_PIN, ENCODER3_B_PIN},
    };
    if (!init_encoders(encoder_pins, sizeof(encoder_pins) / sizeof(encoder_pins[0]))) {
        ESP_LOGI(TAG, "Encoder init failed. Check encoder pin wiring.");
    }
    ESP_LOGI(TAG, "Initializing USB host...");
    UsbHostManager::init();
    #else
    ESP_LOGI(TAG, "CONFIG_IDF_TARGET_ESP32P4 not defined; USB host disabled");
    #endif

    // Store display driver reference for later use (screenshot server after WiFi connects)
    UICommon::setDisplayDriver(&displayDriver);

    // Initialize Screenshot Server (WiFi not connected yet, will initialize after machine selection)
    ESP_LOGI(TAG, "Screenshot server will initialize after WiFi connection...");

    // Initialize comms manager
    ESP_LOGI(TAG, "Initializing CommManager...");
    CommManager::init();

    // Check for auto-import (only if no machines configured)
    ESP_LOGI(TAG, "Checking for settings auto-import...");
    if (SettingsManager::autoImportOnBoot()) {
        ESP_LOGI(TAG, "Settings imported successfully! Restarting...");
        delay(2000);  // Give user time to see serial message
        ESP.restart();
    }

    // Show splash screen
    ESP_LOGI(TAG, "Showing splash screen...");
    UISplash::show(displayDriver.getDisplay());

    // Check if machine selection should be shown
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, true);  // Read-only
    bool show_machine_select = prefs.getBool("show_mach_sel", true);  // Default to true
    prefs.end();
    
    Serial.printf("Main: show_mach_sel preference = %d\n", show_machine_select);
    
    if (show_machine_select) {
        MachineConfig machines[MAX_MACHINES];
        MachineConfigManager::loadMachines(machines);

        int configured_count = 0;
        int only_machine_index = -1;
        for (int i = 0; i < MAX_MACHINES; i++) {
            if (!machines[i].is_configured) {
                continue;
            }
            configured_count++;
            only_machine_index = i;
        }

        if (configured_count == 1 && only_machine_index >= 0) {
            MachineConfigManager::setSelectedMachineIndex(only_machine_index);
            Serial.printf("Auto-selected only configured machine: %s\n", machines[only_machine_index].name);
            UICommon::createMainUI();
        } else {
            // Show machine selection screen
            ESP_LOGI(TAG, "Showing machine selection screen...");
            UIMachineSelect::show(displayDriver.getDisplay());
        }
    } else {
        // Auto-load first configured machine
        ESP_LOGI(TAG, "Auto-loading first machine...");
        MachineConfig machines[MAX_MACHINES];
        MachineConfigManager::loadMachines(machines);
        
        // Find first configured machine
        int first_machine_index = -1;
        for (int i = 0; i < MAX_MACHINES; i++) {
            if (machines[i].is_configured) {
                first_machine_index = i;
                break;
            }
        }
        
        if (first_machine_index >= 0) {
            // Set as selected machine and initialize UI
            MachineConfigManager::setSelectedMachineIndex(first_machine_index);
            Serial.printf("Auto-selected machine: %s\n", machines[first_machine_index].name);
            
            // Initialize main UI directly
            UICommon::createMainUI();
        } else {
            // No machines configured, show selection screen anyway
            ESP_LOGI(TAG, "No machines configured, showing selection screen...");
            UIMachineSelect::show(displayDriver.getDisplay());
        }
    }
}

void loop()
{
    // Handle screenshot server web requests
    ScreenshotServer::handleClient();
    
    // Handle machine comms events
    CommManager::loop();

    // Handle USB HID events (qtdial)
    UsbHostManager::poll();
    QtdialButtonMappingManager::updateHeldButtons();
    
    // Check for connection timeout (non-blocking)
    UICommon::checkConnectionTimeout();
    
    // Check for pending file list refresh (from Files tab delete callback)
    UITabFiles::checkPendingRefresh();
    
    // Update UI from machine status (every 250ms)
    static uint32_t lastUIUpdate = 0;
    uint32_t currentMillis = millis();
    if (currentMillis - lastUIUpdate >= 250) {
        lastUIUpdate = currentMillis;
        
        bool machine_connected = CommManager::isConnected();
        bool wifi_connected = false;
        MachineConfig selected_config;
        if (MachineConfigManager::getSelectedMachine(selected_config)) {
            if (selected_config.connection_type == CONN_WIRELESS) {
                #if FT_WIFI_ENABLED
                wifi_connected = WifiManager::isConnected();
                #else
                wifi_connected = false;
                #endif
            } else {
                wifi_connected = machine_connected;
            }
        }
        
        // Update connection status symbols (always update, even if not connected)
        UICommon::updateConnectionStatus(machine_connected, wifi_connected);
        
        // Update About tab screenshot server URL (in case WiFi status changed)
        UITabSettingsAbout::update();
        
        // Only update other status info if machine is connected
        if (machine_connected) {
            const FluidNCStatus& status = CommManager::getStatus();
        
            // Update status bar
            const char* state_str = "IDLE";
            switch (status.state) {
                case STATE_IDLE: state_str = "IDLE"; break;
                case STATE_RUN: state_str = "RUN"; break;
                case STATE_HOLD: state_str = "HOLD"; break;
                case STATE_JOG: state_str = "JOG"; break;
                case STATE_ALARM: state_str = "ALARM"; break;
                case STATE_DOOR: state_str = "DOOR"; break;
                case STATE_CHECK: state_str = "CHECK"; break;
                case STATE_HOME: state_str = "HOME"; break;
                case STATE_SLEEP: state_str = "SLEEP"; break;
                default: state_str = "DISCONNECTED"; break;
            }
            
            UICommon::updateMachineState(state_str);
            UICommon::updateMachinePosition(status.mpos_x, status.mpos_y, status.mpos_z);
            UICommon::updateWorkPosition(status.wpos_x, status.wpos_y, status.wpos_z);
            
            // Check for HOLD/ALARM state and show popups if needed
            UICommon::checkStatePopups(status.state, status.last_message);
            
            // Update Control Actions pause/resume button based on machine state
            UITabControlActions::updatePauseButton(status.state);
            
            // Update Status tab
            UITabStatus::updateState(state_str);
            UITabStatus::updateWorkPosition(status.wpos_x, status.wpos_y, status.wpos_z);
            UITabStatus::updateMachinePosition(status.mpos_x, status.mpos_y, status.mpos_z);
            UITabStatus::updateFeedRate(status.feed_rate, status.feed_override);
            UITabStatus::updateRapidOverride(status.rapid_override);
            UITabStatus::updateRapidJogState(UITabControlJog::isRapidFeedEnabled());
            UITabStatus::updateSpindle(status.spindle_speed, status.spindle_override);
            UITabStatus::updateModalStates(status.modal_wcs, status.modal_plane, status.modal_distance,
                                        status.modal_units, status.modal_motion, status.modal_feedrate,
                                        status.modal_spindle, status.modal_coolant, status.modal_tool);
            UITabStatus::updateFileProgress(status.is_sd_printing, status.sd_percent, 
                                        status.sd_filename, status.sd_elapsed_ms);
            UITabStatus::updateMessage(status.last_message);
            
            // Update Macros tab progress (only when a macro from that tab is running)
            static bool macro_print_started = false;  // Track if SD print actually started
            static unsigned long completion_display_start = 0;  // Track 100% display time
            static unsigned long macro_start_time = 0;  // Track when macro button was clicked
            static const unsigned long COMPLETION_DISPLAY_MS = 2000;  // Show 100% for 2 seconds
            static const unsigned long FAST_MACRO_TIMEOUT_MS = 1000;  // If no SD activity within 1s, assume macro completed
            bool is_macro_running = UITabMacros::isMacroRunning();
            
            // Detect when a new macro starts (transition from not running to running)
            static bool was_macro_running = false;
            if (is_macro_running && !was_macro_running) {
                // New macro just started
                macro_start_time = millis();
                macro_print_started = false;
                Serial.printf("[Main] New macro started, tracking start time\n");
            }
            was_macro_running = is_macro_running;
            
            if (status.is_sd_printing && status.sd_percent > 0 && is_macro_running) {
                macro_print_started = true;  // Mark that print has started
                completion_display_start = 0;  // Reset completion timer while printing
                Serial.printf("[Main] Showing progress: printing=%d, percent=%.1f, macro_running=%d\n", 
                    status.is_sd_printing, status.sd_percent, is_macro_running);
                // Use the stored macro name (not the SD filename)
                UITabMacros::updateProgress((int)status.sd_percent, UITabMacros::getRunningMacroName(), status.last_message);
                UITabMacros::showProgress();
            } else {
                // Check if macro completed (either we saw SD activity that stopped, or it was too fast)
                bool macro_completed = false;
                
                if (is_macro_running && macro_print_started && !status.is_sd_printing) {
                    // Normal case: SD print started and then stopped
                    macro_completed = true;
                    Serial.printf("[Main] Macro completed (normal)\n");
                } else if (is_macro_running && !macro_print_started && macro_start_time > 0 && 
                          (millis() - macro_start_time >= FAST_MACRO_TIMEOUT_MS)) {
                    // Fast macro case: never saw SD activity, but enough time passed
                    macro_completed = true;
                    Serial.printf("[Main] Macro completed (fast, no SD activity detected)\n");
                }
                
                if (macro_completed) {
                    // Macro completed - start 2-second display timer if not already started
                    if (completion_display_start == 0) {
                        completion_display_start = millis();
                        Serial.printf("[Main] Showing 100%% for 2 seconds\n");
                        // Show 100% with the macro name
                        UITabMacros::updateProgress(100, UITabMacros::getRunningMacroName(), "Complete");
                        UITabMacros::showProgress();
                    } else {
                        // Check if 2 seconds have elapsed
                        if (millis() - completion_display_start >= COMPLETION_DISPLAY_MS) {
                            Serial.printf("[Main] Completion display timeout, clearing macro\n");
                            UITabMacros::clearRunningMacro();
                            macro_print_started = false;
                            completion_display_start = 0;
                            macro_start_time = 0;
                            UITabMacros::hideProgress();
                        } else {
                            // Still within 2-second window, keep showing 100%
                            UITabMacros::showProgress();
                        }
                    }
                } else if (is_macro_running && !macro_print_started && macro_start_time > 0) {
                    // Macro is running but hasn't started SD print yet - keep showing progress
                    UITabMacros::showProgress();
                } else {
                    // No macro running, hide progress
                    UITabMacros::hideProgress();
                }
            }
            
            // Update Override tab
            UITabControlOverride::updateValues(status.feed_override, status.rapid_override, status.spindle_override);
            
            // Update power manager with current machine state
            PowerManager::update(status.state);

            // Sync CNC status to USB HID devices
            sendHidStatus(status, true);
        } else {
            // Machine disconnected - show OFFLINE state and reset all values to dashes
            UICommon::updateMachineState("OFFLINE");
            UICommon::updateMachinePosition(-9999.0f, -9999.0f, -9999.0f);  // Triggers dash display
            UICommon::updateWorkPosition(-9999.0f, -9999.0f, -9999.0f);     // Triggers dash display
            
            // Update Status tab with OFFLINE state and reset all values
            UITabStatus::updateState("OFFLINE");
            UITabStatus::updateWorkPosition(-9999.0f, -9999.0f, -9999.0f);
            UITabStatus::updateMachinePosition(-9999.0f, -9999.0f, -9999.0f);
            UITabStatus::updateFeedRate(-9999.0f, -9999.0f);  // Reset feed rate and override
            UITabStatus::updateRapidOverride(-9999.0f);        // Reset rapid override
            UITabStatus::updateRapidJogState(UITabControlJog::isRapidFeedEnabled());
            UITabStatus::updateSpindle(-9999.0f, -9999.0f);    // Reset spindle and override
            UITabStatus::updateModalStates("---", "---", "---", "---", "---", "---", "---", "---", "---");
            
            // Update power manager with OFFLINE state (treat as IDLE for power management)
            PowerManager::update(STATE_IDLE);

            // Send offline status to USB HID devices
            FluidNCStatus offline_status{};
            sendHidStatus(offline_status, false);
        }
    }
    
    // Check machine connection timeout
    UICommon::checkConnectionTimeout();
    
    // Update Terminal tab (batched UI updates every 100ms)
    UITabTerminal::update();
    
    // Update LVGL tick (CRITICAL for timers and input device polling!)
    static uint32_t lastTick = 0;
    lv_tick_inc(currentMillis - lastTick);
    lastTick = currentMillis;
    
    lv_timer_handler();
    delay(5);
    
    // Status update every 5 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        Serial.printf("[%lu] LVGL running, Free heap: %d, Machine: %s\n",
                      millis()/1000, ESP.getFreeHeap(),
                      CommManager::isConnected() ? "Connected" : "Disconnected");
    }
}
