#include "ui/ui_common.h"
#include "ui/ui_tabs.h"
#include "ui/ui_theme.h"
#include "ui/machine_config.h"
#include "ui/ui_machine_select.h"
#include "fluidnc_client.h"
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>

// Static member initialization
lv_display_t *UICommon::display = nullptr;
lv_obj_t *UICommon::status_bar = nullptr;
lv_obj_t *UICommon::status_bar_left_area = nullptr;
lv_obj_t *UICommon::status_bar_right_area = nullptr;
lv_obj_t *UICommon::machine_select_dialog = nullptr;
lv_obj_t *UICommon::lbl_modal_states = nullptr;
lv_obj_t *UICommon::lbl_status = nullptr;

// Connection status labels
lv_obj_t *UICommon::lbl_machine_symbol = nullptr;
lv_obj_t *UICommon::lbl_machine_name = nullptr;
lv_obj_t *UICommon::lbl_wifi_symbol = nullptr;
lv_obj_t *UICommon::lbl_wifi_name = nullptr;

// Work Position labels
lv_obj_t *UICommon::lbl_wpos_label = nullptr;
lv_obj_t *UICommon::lbl_wpos_x = nullptr;
lv_obj_t *UICommon::lbl_wpos_y = nullptr;
lv_obj_t *UICommon::lbl_wpos_z = nullptr;

// Machine Position labels
lv_obj_t *UICommon::lbl_mpos_label = nullptr;
lv_obj_t *UICommon::lbl_mpos_x = nullptr;
lv_obj_t *UICommon::lbl_mpos_y = nullptr;
lv_obj_t *UICommon::lbl_mpos_z = nullptr;

// Cached values for delta checking
float UICommon::last_wpos_x = -9999.0f;
float UICommon::last_wpos_y = -9999.0f;
float UICommon::last_wpos_z = -9999.0f;
float UICommon::last_mpos_x = -9999.0f;
float UICommon::last_mpos_y = -9999.0f;
float UICommon::last_mpos_z = -9999.0f;

// Event handler for status bar left area click (go to Status tab)
static void status_bar_left_click_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Switch to Status tab (index 0)
        lv_obj_t *tabview = UITabs::getTabview();
        if (tabview) {
            lv_tabview_set_active(tabview, 0, LV_ANIM_ON);
        }
    }
}

// Event handler for status bar right area click (machine selection confirmation)
static void status_bar_right_click_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        UICommon::showMachineSelectConfirmDialog();
    }
}

// Event handler for confirming machine selection change
static void on_machine_select_confirm(lv_event_t *e) {
    Serial.println("UICommon: Restarting to change machine...");
    UICommon::hideMachineSelectConfirmDialog();
    
    // Show restart message
    lv_obj_t *restart_label = lv_label_create(lv_screen_active());
    lv_label_set_text(restart_label, "Restarting...");
    lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(restart_label, UITheme::UI_INFO, 0);
    lv_obj_center(restart_label);
    
    // Force display update
    lv_timer_handler();
    delay(500);
    
    // Restart the ESP32
    ESP.restart();
}

// Event handler for canceling machine selection change
static void on_machine_select_cancel(lv_event_t *e) {
    Serial.println("UICommon: Machine selection cancelled");
    UICommon::hideMachineSelectConfirmDialog();
}

void UICommon::init(lv_display_t *disp) {
    display = disp;
}

void UICommon::createMainUI() {
    Serial.println("UICommon: Creating main UI");
    
    // Create main screen
    lv_obj_t *main_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(main_screen, UITheme::BG_DARKER, LV_PART_MAIN);
    
    // Load the new screen
    lv_scr_load(main_screen);
    
    // Create status bar
    createStatusBar();
    
    // Create all tabs
    UITabs::createTabs();
    
    // Connect to FluidNC using selected machine
    MachineConfig config;
    if (MachineConfigManager::getSelectedMachine(config)) {
        Serial.printf("UICommon: Connecting to FluidNC at %s:%d\n", 
                     config.fluidnc_url, config.websocket_port);
        FluidNCClient::connect(config);
    } else {
        Serial.println("UICommon: Warning - No machine selected, cannot connect to FluidNC");
    }
    
    Serial.println("UICommon: Main UI created");
}

void UICommon::createStatusBar() {
    // Create status bar at bottom (always visible) - 2 lines with CNC info
    status_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(status_bar, SCREEN_WIDTH, STATUS_BAR_HEIGHT);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, UITheme::BG_DARK, LV_PART_MAIN);
    lv_obj_set_style_border_color(status_bar, UITheme::BORDER_MEDIUM, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(status_bar, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_radius(status_bar, 0, LV_PART_MAIN); // No rounded corners
    lv_obj_set_style_pad_all(status_bar, 5, LV_PART_MAIN);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create left clickable area (goes to Status tab)
    status_bar_left_area = lv_obj_create(status_bar);
    lv_obj_set_size(status_bar_left_area, 550, STATUS_BAR_HEIGHT - 10);  // Left 550px
    lv_obj_align(status_bar_left_area, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_left_area, LV_OPA_TRANSP, 0);  // Transparent
    lv_obj_set_style_border_width(status_bar_left_area, 0, 0);
    lv_obj_clear_flag(status_bar_left_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(status_bar_left_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_bar_left_area, status_bar_left_click_handler, LV_EVENT_CLICKED, NULL);
    
    // Create right clickable area (goes to machine selection with confirmation)
    status_bar_right_area = lv_obj_create(status_bar);
    lv_obj_set_size(status_bar_right_area, 240, STATUS_BAR_HEIGHT - 10);  // Right 240px
    lv_obj_align(status_bar_right_area, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_right_area, LV_OPA_TRANSP, 0);  // Transparent
    lv_obj_set_style_border_width(status_bar_right_area, 0, 0);
    lv_obj_clear_flag(status_bar_right_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(status_bar_right_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_bar_right_area, status_bar_right_click_handler, LV_EVENT_CLICKED, NULL);

    // Left side: Large Status (centered vertically)
    lbl_status = lv_label_create(status_bar);
    lv_label_set_text(lbl_status, "IDLE");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_status, UITheme::STATE_IDLE, 0);
    lv_obj_align(lbl_status, LV_ALIGN_LEFT_MID, 5, 0);

    // Middle: Work Position (line 1) and Machine Position (line 2)
    // Work Position - Line 1
    lv_obj_t *lbl_wpos_label = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_label, "WPos:");
    lv_obj_set_style_text_font(lbl_wpos_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_label, UITheme::POS_WORK, 0);  // Orange - primary data
    lv_obj_align(lbl_wpos_label, LV_ALIGN_TOP_MID, -198, 3);  // -210 + 2 + 10 = -198

    lbl_wpos_x = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_x, "X:0000.000");
    lv_obj_set_style_text_font(lbl_wpos_x, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_x, UITheme::AXIS_X, 0);
    lv_obj_align(lbl_wpos_x, LV_ALIGN_TOP_MID, -110, 3);  // -120 + 10 = -110

    lbl_wpos_y = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_y, "Y:0000.000");
    lv_obj_set_style_text_font(lbl_wpos_y, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_y, UITheme::AXIS_Y, 0);
    lv_obj_align(lbl_wpos_y, LV_ALIGN_TOP_MID, 0, 3);  // -10 + 10 = 0

    lbl_wpos_z = lv_label_create(status_bar);
    lv_label_set_text(lbl_wpos_z, "Z:0000.000");
    lv_obj_set_style_text_font(lbl_wpos_z, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wpos_z, UITheme::AXIS_Z, 0);
    lv_obj_align(lbl_wpos_z, LV_ALIGN_TOP_MID, 110, 3);  // 100 + 10 = 110

    // Machine Position - Line 2
    lbl_mpos_label = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_label, "MPos:");
    lv_obj_set_style_text_font(lbl_mpos_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_label, UITheme::POS_MACHINE, 0);  // Cyan - secondary data
    lv_obj_align(lbl_mpos_label, LV_ALIGN_BOTTOM_MID, -198, -3);  // -210 + 2 + 10 = -198

    lbl_mpos_x = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_x, "X:0000.000");
    lv_obj_set_style_text_font(lbl_mpos_x, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_x, UITheme::AXIS_X, 0);
    lv_obj_align(lbl_mpos_x, LV_ALIGN_BOTTOM_MID, -110, -3);  // -120 + 10 = -110

    lbl_mpos_y = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_y, "Y:0000.000");
    lv_obj_set_style_text_font(lbl_mpos_y, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_y, UITheme::AXIS_Y, 0);
    lv_obj_align(lbl_mpos_y, LV_ALIGN_BOTTOM_MID, 0, -3);  // -10 + 10 = 0

    lbl_mpos_z = lv_label_create(status_bar);
    lv_label_set_text(lbl_mpos_z, "Z:0000.000");
    lv_obj_set_style_text_font(lbl_mpos_z, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_mpos_z, UITheme::AXIS_Z, 0);
    lv_obj_align(lbl_mpos_z, LV_ALIGN_BOTTOM_MID, 110, -3);  // 100 + 10 = 110

    // Right side Line 1: Machine name with symbol
    // Get selected machine from config manager
    MachineConfig selected_machine;
    
    if (MachineConfigManager::getSelectedMachine(selected_machine)) {
        const char *symbol = (selected_machine.connection_type == CONN_WIRELESS) ? LV_SYMBOL_WIFI : LV_SYMBOL_USB;
        
        // Symbol (will be colored based on connection status)
        lbl_machine_symbol = lv_label_create(status_bar);
        lv_label_set_text(lbl_machine_symbol, symbol);
        lv_obj_set_style_text_font(lbl_machine_symbol, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_machine_symbol, UITheme::STATE_ALARM, 0);  // Start red
        lv_obj_align(lbl_machine_symbol, LV_ALIGN_TOP_RIGHT, -5, 3);
        
        // Machine name
        lbl_machine_name = lv_label_create(status_bar);
        lv_label_set_text(lbl_machine_name, selected_machine.name);
        lv_obj_set_style_text_font(lbl_machine_name, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_machine_name, UITheme::ACCENT_PRIMARY, 0);
        lv_obj_align_to(lbl_machine_name, lbl_machine_symbol, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    } else {
        lbl_modal_states = lv_label_create(status_bar);
        lv_label_set_text(lbl_modal_states, "No Machine");
        lv_obj_set_style_text_font(lbl_modal_states, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_modal_states, UITheme::ACCENT_PRIMARY, 0);
        lv_obj_align(lbl_modal_states, LV_ALIGN_TOP_RIGHT, -5, 3);
    }

    // Right side Line 2: WiFi network
    // WiFi symbol (will be colored based on connection status)
    lbl_wifi_symbol = lv_label_create(status_bar);
    lv_label_set_text(lbl_wifi_symbol, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_wifi_symbol, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wifi_symbol, WiFi.isConnected() ? UITheme::STATE_IDLE : UITheme::STATE_ALARM, 0);
    lv_obj_align(lbl_wifi_symbol, LV_ALIGN_BOTTOM_RIGHT, -5, -3);
    
    // WiFi network name
    String wifi_ssid = WiFi.isConnected() ? WiFi.SSID() : "Not Connected";
    lbl_wifi_name = lv_label_create(status_bar);
    lv_label_set_text(lbl_wifi_name, wifi_ssid.c_str());
    lv_obj_set_style_text_font(lbl_wifi_name, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_wifi_name, UITheme::UI_INFO, 0);
    lv_obj_align_to(lbl_wifi_name, lbl_wifi_symbol, LV_ALIGN_OUT_LEFT_MID, -5, 0);
}

void UICommon::updateModalStates(const char *text) {
    // Check if status bar exists and label is valid
    if (status_bar && lbl_modal_states) {
        lv_label_set_text(lbl_modal_states, text);
    }
}

void UICommon::updateMachinePosition(float x, float y, float z) {
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_mpos_x && y == last_mpos_y && z == last_mpos_z) {
        return;
    }
    
    char buf[16];
    
    if (status_bar && lbl_mpos_x && x != last_mpos_x) {
        snprintf(buf, sizeof(buf), "X:%04.3f", x);
        lv_label_set_text(lbl_mpos_x, buf);
        last_mpos_x = x;
    }
    if (status_bar && lbl_mpos_y && y != last_mpos_y) {
        snprintf(buf, sizeof(buf), "Y:%04.3f", y);
        lv_label_set_text(lbl_mpos_y, buf);
        last_mpos_y = y;
    }
    if (status_bar && lbl_mpos_z && z != last_mpos_z) {
        snprintf(buf, sizeof(buf), "Z:%04.3f", z);
        lv_label_set_text(lbl_mpos_z, buf);
        last_mpos_z = z;
    }
}

void UICommon::updateWorkPosition(float x, float y, float z) {
    // Only update if values changed (avoid unnecessary redraws)
    if (x == last_wpos_x && y == last_wpos_y && z == last_wpos_z) {
        return;
    }
    
    char buf[16];
    
    if (status_bar && lbl_wpos_x && x != last_wpos_x) {
        snprintf(buf, sizeof(buf), "X:%04.3f", x);
        lv_label_set_text(lbl_wpos_x, buf);
        last_wpos_x = x;
    }
    if (status_bar && lbl_wpos_y && y != last_wpos_y) {
        snprintf(buf, sizeof(buf), "Y:%04.3f", y);
        lv_label_set_text(lbl_wpos_y, buf);
        last_wpos_y = y;
    }
    if (status_bar && lbl_wpos_z && z != last_wpos_z) {
        snprintf(buf, sizeof(buf), "Z:%04.3f", z);
        lv_label_set_text(lbl_wpos_z, buf);
        last_wpos_z = z;
    }
}

void UICommon::updateMachineState(const char *state) {
    if (status_bar && lbl_status) {
        lv_label_set_text(lbl_status, state);
        
        // Color code the status (state is already uppercase)
        if (strcmp(state, "IDLE") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_IDLE, 0);
        } else if (strcmp(state, "RUN") == 0 || strcmp(state, "JOG") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_RUN, 0);
        } else if (strcmp(state, "ALARM") == 0) {
            lv_obj_set_style_text_color(lbl_status, UITheme::STATE_ALARM, 0);
        } else {
            lv_obj_set_style_text_color(lbl_status, UITheme::UI_WARNING, 0);
        }
    }
}

void UICommon::updateConnectionStatus(bool machine_connected, bool wifi_connected) {
    // Update machine symbol color
    if (lbl_machine_symbol) {
        lv_obj_set_style_text_color(lbl_machine_symbol, 
            machine_connected ? UITheme::STATE_IDLE : UITheme::STATE_ALARM, 0);
    }
    
    // Update WiFi symbol color
    if (lbl_wifi_symbol) {
        lv_obj_set_style_text_color(lbl_wifi_symbol, 
            wifi_connected ? UITheme::STATE_IDLE : UITheme::STATE_ALARM, 0);
    }
}

void UICommon::showMachineSelectConfirmDialog() {
    // Create modal background
    machine_select_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(machine_select_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(machine_select_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(machine_select_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(machine_select_dialog, 0, 0);
    lv_obj_clear_flag(machine_select_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(machine_select_dialog);
    lv_obj_set_size(content, 500, 220);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_set_style_pad_gap(content, 15, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Icon and title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Change Machine?");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_PRIMARY, 0);
    
    // Message line 1
    lv_obj_t *msg1_label = lv_label_create(content);
    lv_label_set_text(msg1_label, "This will restart the controller.");
    lv_obj_set_style_text_font(msg1_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(msg1_label, UITheme::TEXT_LIGHT, 0);
    
    // Message line 2
    lv_obj_t *msg2_label = lv_label_create(content);
    lv_label_set_text(msg2_label, "Continue?");
    lv_obj_set_style_text_font(msg2_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(msg2_label, UITheme::UI_WARNING, 0);
    
    // Button container
    lv_obj_t *btn_container = lv_obj_create(content);
    lv_obj_set_size(btn_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Cancel button
    lv_obj_t *cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(cancel_btn, 180, 50);
    lv_obj_set_style_bg_color(cancel_btn, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(cancel_btn, on_machine_select_cancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);
    
    // Confirm button
    lv_obj_t *confirm_btn = lv_btn_create(btn_container);
    lv_obj_set_size(confirm_btn, 180, 50);
    lv_obj_set_style_bg_color(confirm_btn, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(confirm_btn, on_machine_select_confirm, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *confirm_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_label, LV_SYMBOL_POWER " Restart");
    lv_obj_set_style_text_font(confirm_label, &lv_font_montserrat_18, 0);
    lv_obj_center(confirm_label);
}

void UICommon::hideMachineSelectConfirmDialog() {
    if (machine_select_dialog) {
        lv_obj_del(machine_select_dialog);
        machine_select_dialog = nullptr;
    }
}
