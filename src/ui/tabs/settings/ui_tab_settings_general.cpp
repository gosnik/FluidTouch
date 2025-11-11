#include "ui/tabs/settings/ui_tab_settings_general.h"
#include "ui/ui_theme.h"
#include "config.h"
#include <Preferences.h>

// Global references for UI elements
static lv_obj_t *status_label = NULL;
static lv_obj_t *show_machine_select_switch = NULL;
static lv_obj_t *folders_on_top_switch = NULL;

// Forward declarations for event handlers
static void btn_save_general_event_handler(lv_event_t *e);
static void btn_reset_event_handler(lv_event_t *e);

void UITabSettingsGeneral::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Disable scrolling for fixed layout
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // Load current preferences
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, true);  // Read-only
    bool show_machine_select = prefs.getBool("show_mach_sel", true);  // Default to true
    bool folders_on_top = prefs.getBool("folders_on_top", false);  // Default to false (folders at bottom)
    prefs.end();
    
    Serial.printf("UITabSettingsGeneral: Loaded show_mach_sel=%d, folders_on_top=%d\n", show_machine_select, folders_on_top);
    
    // === Machine Selection Section ===
    lv_obj_t *section_title = lv_label_create(tab);
    lv_label_set_text(section_title, "MACHINE SELECTION");
    lv_obj_set_style_text_font(section_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(section_title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(section_title, 20, 20);
    
    // Show label and switch on same line
    lv_obj_t *machine_sel_label = lv_label_create(tab);
    lv_label_set_text(machine_sel_label, "Show:");
    lv_obj_set_style_text_font(machine_sel_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(machine_sel_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(machine_sel_label, 20, 70);  // 20 + 40 (title spacing) + 12 (vertical alignment)
    
    show_machine_select_switch = lv_switch_create(tab);
    lv_obj_set_pos(show_machine_select_switch, 140, 65);  // 20 + 40 (title spacing) + 7 (switch alignment)
    if (show_machine_select) {
        lv_obj_add_state(show_machine_select_switch, LV_STATE_CHECKED);
    }
    
    // Description text
    lv_obj_t *desc_label = lv_label_create(tab);
    lv_label_set_text(desc_label, "When disabled, the first configured\nmachine will be loaded automatically\nat startup.");
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(desc_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(desc_label, 20, 107);  // 20 + 40 (title) + 40 (switch row) + 7 (spacing)
    
    // === Files Section (Second Column) ===
    lv_obj_t *files_section_title = lv_label_create(tab);
    lv_label_set_text(files_section_title, "FILES");
    lv_obj_set_style_text_font(files_section_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(files_section_title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(files_section_title, 360, 20);  // Second column, aligned with Machine Selection title
    
    // Folders on top label and switch
    lv_obj_t *folders_label = lv_label_create(tab);
    lv_label_set_text(folders_label, "Folders on Top:");
    lv_obj_set_style_text_font(folders_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(folders_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(folders_label, 360, 70);  // Second column, aligned with Machine Selection switch row
    
    folders_on_top_switch = lv_switch_create(tab);
    lv_obj_set_pos(folders_on_top_switch, 560, 65);  // Aligned with Machine Selection switch
    if (folders_on_top) {
        lv_obj_add_state(folders_on_top_switch, LV_STATE_CHECKED);
    }
    
    // Description text for folders setting
    lv_obj_t *folders_desc_label = lv_label_create(tab);
    lv_label_set_text(folders_desc_label, "When enabled, folders appear at the top\nof the file list instead of the bottom.");
    lv_obj_set_style_text_font(folders_desc_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(folders_desc_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(folders_desc_label, 360, 107);  // Aligned with Machine Selection description
    
    // === Action Buttons (positioned at bottom with 20px margins) ===
    // Save button
    lv_obj_t *btn_save = lv_button_create(tab);
    lv_obj_set_size(btn_save, 180, 50);
    lv_obj_set_pos(btn_save, 20, 280);  // 360px (tab height) - 50px (button) - 30px (margin) = 280px
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, LV_PART_MAIN);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save Settings");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, btn_save_general_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Reset to defaults button
    lv_obj_t *btn_reset = lv_button_create(tab);
    lv_obj_set_size(btn_reset, 180, 50);
    lv_obj_set_pos(btn_reset, 220, 280);  // Same vertical position, 200px gap from Save button
    lv_obj_set_style_bg_color(btn_reset, UITheme::BG_BUTTON, LV_PART_MAIN);
    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Reset Defaults");
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(btn_reset, btn_reset_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Status label (positioned above buttons)
    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(status_label, 20, 335);
}

// Save button event handler
static void btn_save_general_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Save preferences
        bool show_machine_select = lv_obj_has_state(show_machine_select_switch, LV_STATE_CHECKED);
        bool folders_on_top = lv_obj_has_state(folders_on_top_switch, LV_STATE_CHECKED);
        
        Serial.printf("UITabSettingsGeneral: Saving show_mach_sel=%d, folders_on_top=%d\n", show_machine_select, folders_on_top);
        
        Preferences prefs;
        if (!prefs.begin(PREFS_SYSTEM_NAMESPACE, false)) {  // Read-write
            Serial.println("UITabSettingsGeneral: ERROR - Failed to open Preferences for writing!");
            if (status_label != NULL) {
                lv_label_set_text(status_label, "Error: Failed to save!");
                lv_obj_set_style_text_color(status_label, UITheme::STATE_ALARM, 0);
            }
            return;
        }
        
        prefs.putBool("show_mach_sel", show_machine_select);
        prefs.putBool("folders_on_top", folders_on_top);
        prefs.end();
        
        // Verify it was saved
        prefs.begin(PREFS_SYSTEM_NAMESPACE, true);
        bool verified_machine = prefs.getBool("show_mach_sel", true);
        bool verified_folders = prefs.getBool("folders_on_top", false);
        prefs.end();
        
        Serial.printf("UITabSettingsGeneral: Verified show_mach_sel=%d, folders_on_top=%d\n", verified_machine, verified_folders);
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Settings saved!");
            lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
        }
    }
}

// Reset to defaults button event handler
static void btn_reset_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Reset to defaults (show machine selection enabled, folders at bottom)
        lv_obj_add_state(show_machine_select_switch, LV_STATE_CHECKED);
        lv_obj_clear_state(folders_on_top_switch, LV_STATE_CHECKED);  // Default: folders at bottom
        
        if (status_label != NULL) {
            lv_label_set_text(status_label, "Reset to defaults");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        Serial.println("General Reset button clicked");
    }
}
