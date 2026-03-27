#include "ui/tabs/ui_tab_macros.h"
#include "ui/ui_theme.h"
#include "ui/machine_config.h"
#include "ui/upload_manager.h"
#include "config.h"
#include "core/comm_manager.h"
#include "env/platform.h"
#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#if defined(FT_PLATFORM_PC)
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <SDL2/SDL.h>
#endif

namespace {
#if defined(FT_PLATFORM_PC) || defined(FT_PLATFORM_RPI)
const char *macroStoragePathDisplay() {
    return EnvPlatform::macroStorageDisplayPath();
}

const char *macroStoragePathCommand() {
    return EnvPlatform::macroStorageCommandPath();
}

const char *macroRunCommandPrefix() {
    return EnvPlatform::macroRunCommandPrefix();
}

const char *macroNoMacroMessage() {
    return EnvPlatform::macroNoMacroMessage();
}
#else
const char *macroStoragePathDisplay() {
    return "/sd/fluidtouch/macros/";
}

const char *macroStoragePathCommand() {
    return "/sd/fluidtouch/macros";
}

const char *macroRunCommandPrefix() {
    return "$SD/Run=";
}

const char *macroNoMacroMessage() {
    return "No macros configured.\n\nClick " LV_SYMBOL_EDIT
           " Edit to add macros.\n\nMacro files must be on machine SD card in /fluidtouch/macros directory.";
}
#endif
}  // namespace

namespace {

std::vector<std::string> &recordedCommandsStorage() {
    static std::vector<std::string> commands;
    return commands;
}

std::vector<std::string> &macroFilesStorage() {
    static std::vector<std::string> files;
    return files;
}

}  // namespace

// Static member initialization
lv_obj_t *UITabMacros::parent_tab = nullptr;
lv_obj_t *UITabMacros::macro_container = nullptr;
lv_obj_t *UITabMacros::btn_edit = nullptr;
lv_obj_t *UITabMacros::btn_record = nullptr;
lv_obj_t *UITabMacros::btn_add = nullptr;
lv_obj_t *UITabMacros::btn_done = nullptr;
lv_obj_t *UITabMacros::lbl_empty_message = nullptr;
lv_obj_t *UITabMacros::progress_container = nullptr;
lv_obj_t *UITabMacros::lbl_macro_name = nullptr;
lv_obj_t *UITabMacros::bar_progress = nullptr;
lv_obj_t *UITabMacros::lbl_percent = nullptr;
lv_obj_t *UITabMacros::lbl_message = nullptr;
char UITabMacros::running_macro_name[32] = "";
bool UITabMacros::is_edit_mode = false;
MacroConfig UITabMacros::macros[MAX_MACROS];
lv_obj_t *UITabMacros::macro_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::edit_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::up_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::down_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::delete_buttons[MAX_MACROS] = {nullptr};
lv_obj_t *UITabMacros::config_dialog = nullptr;
lv_obj_t *UITabMacros::config_name_textarea = nullptr;
lv_obj_t *UITabMacros::config_path_dropdown = nullptr;
lv_obj_t *UITabMacros::config_color_buttons[8] = {nullptr};
int UITabMacros::selected_color_index = 0;
lv_obj_t *UITabMacros::keyboard = nullptr;
int UITabMacros::editing_index = -1;
lv_obj_t *UITabMacros::delete_dialog = nullptr;
lv_obj_t *UITabMacros::record_save_dialog = nullptr;
lv_obj_t *UITabMacros::record_name_textarea = nullptr;
lv_obj_t *UITabMacros::repeat_dialog = nullptr;
lv_obj_t *UITabMacros::repeat_count_textarea = nullptr;
int UITabMacros::repeat_macro_index = -1;
bool UITabMacros::is_recording = false;
bool UITabMacros::suppress_next_macro_click = false;

void UITabMacros::create(lv_obj_t *tab) {
    parent_tab = tab;
    is_edit_mode = false;
    
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Remove default padding on tab
    lv_obj_set_style_pad_all(tab, 0, 0);

    // Load macros from preferences
    loadMacros();

    // PROGRESS DISPLAY - Top area (hidden by default, shown during macro execution)
    progress_container = lv_obj_create(tab);
    lv_obj_set_size(progress_container, UI_SCALE_X(520), UI_SCALE_Y(65));  // Keep clear of Rec button area
    lv_obj_set_pos(progress_container, UI_SCALE_X(15), UI_SCALE_Y(5));
    lv_obj_set_style_bg_color(progress_container, UITheme::BG_DARKER, 0);
    lv_obj_set_style_border_width(progress_container, 1, 0);
    lv_obj_set_style_border_color(progress_container, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_pad_all(progress_container, UI_SCALE_Y(5), 0);
    lv_obj_clear_flag(progress_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(progress_container, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    // Macro name label
    lbl_macro_name = lv_label_create(progress_container);
    lv_label_set_text(lbl_macro_name, "Running: Macro Name");
    lv_obj_set_style_text_font(lbl_macro_name, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_macro_name, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(lbl_macro_name, 0, 0);
    lv_label_set_long_mode(lbl_macro_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_macro_name, UI_SCALE_X(395));
    
    // Progress bar
    bar_progress = lv_bar_create(progress_container);
    lv_obj_set_size(bar_progress, UI_SCALE_X(390), UI_SCALE_Y(15));
    lv_obj_set_pos(bar_progress, 0, UI_SCALE_Y(20));
    lv_obj_set_style_bg_color(bar_progress, UITheme::BG_BLACK, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_progress, UITheme::UI_SUCCESS, LV_PART_INDICATOR);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    
    // Percentage label
    lbl_percent = lv_label_create(progress_container);
    lv_label_set_text(lbl_percent, "0%");
    lv_obj_set_style_text_font(lbl_percent, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_percent, UITheme::UI_SUCCESS, 0);
    lv_obj_set_pos(lbl_percent, UI_SCALE_X(398), UI_SCALE_Y(18));
    
    // Message label
    lbl_message = lv_label_create(progress_container);
    lv_label_set_text(lbl_message, "");
    lv_obj_set_style_text_font(lbl_message, &lv_font_montserrat_14, 0);  // Increased from 12 to 14
    lv_obj_set_style_text_color(lbl_message, UITheme::UI_INFO, 0);
    lv_obj_set_pos(lbl_message, 0, UI_SCALE_Y(40));
    lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_message, UI_SCALE_X(505));

    // Edit button (upper right corner, absolute positioning on tab)
    btn_edit = lv_btn_create(tab);
    lv_obj_set_size(btn_edit, UI_SCALE_X(120), UI_SCALE_Y(45));
    lv_obj_set_style_bg_color(btn_edit, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(btn_edit, UI_SCALE_X(665), UI_SCALE_Y(15));
    lv_obj_add_event_cb(btn_edit, onEditModeToggle, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *edit_label = lv_label_create(btn_edit);
    lv_label_set_text(edit_label, LV_SYMBOL_EDIT " Edit");
    lv_obj_set_style_text_font(edit_label, &lv_font_montserrat_16, 0);
    lv_obj_center(edit_label);

    btn_record = lv_btn_create(tab);
    lv_obj_set_size(btn_record, UI_SCALE_X(120), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_record, UI_SCALE_X(540), UI_SCALE_Y(15));
    lv_obj_add_event_cb(btn_record, onRecordToggle, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *record_label = lv_label_create(btn_record);
    lv_label_set_text(record_label, "Rec");
    lv_obj_set_style_text_font(record_label, &lv_font_montserrat_16, 0);
    lv_obj_center(record_label);
    updateRecordButtonState();

    // Add button (initially hidden)
    btn_add = lv_btn_create(tab);
    lv_obj_set_size(btn_add, UI_SCALE_X(120), UI_SCALE_Y(45));
    lv_obj_set_style_bg_color(btn_add, UITheme::BTN_PLAY, 0);
    lv_obj_set_pos(btn_add, UI_SCALE_X(540), UI_SCALE_Y(15));  // Left of Done button
    lv_obj_add_event_cb(btn_add, onAddMacro, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_add, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *add_label = lv_label_create(btn_add);
    lv_label_set_text(add_label, LV_SYMBOL_PLUS " Add");
    lv_obj_set_style_text_font(add_label, &lv_font_montserrat_16, 0);
    lv_obj_center(add_label);

    // Done button (initially hidden)
    btn_done = lv_btn_create(tab);
    lv_obj_set_size(btn_done, UI_SCALE_X(120), UI_SCALE_Y(45));
    lv_obj_set_style_bg_color(btn_done, UITheme::BTN_PLAY, 0);
    lv_obj_set_pos(btn_done, UI_SCALE_X(670), UI_SCALE_Y(15));  // Same position as Edit button
    lv_obj_add_event_cb(btn_done, onEditModeToggle, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_done, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *done_label = lv_label_create(btn_done);
    lv_label_set_text(done_label, LV_SYMBOL_OK " Done");
    lv_obj_set_style_text_font(done_label, &lv_font_montserrat_16, 0);
    lv_obj_center(done_label);

    // Macro container for flex layout
    macro_container = lv_obj_create(tab);
    lv_obj_set_size(macro_container, UI_SCALE_X(770), UI_SCALE_Y(270));
    lv_obj_set_style_bg_color(macro_container, UITheme::BG_DARKER, LV_PART_MAIN);
    lv_obj_set_style_border_color(macro_container, UITheme::BORDER_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(macro_container, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(macro_container, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_pad_all(macro_container, 0, 0);
    lv_obj_set_pos(macro_container, UI_SCALE_X(15), UI_SCALE_Y(75));  // Position below Edit button
    lv_obj_clear_flag(macro_container, LV_OBJ_FLAG_SCROLLABLE);

    // Empty message label (shown when no macros configured)
    lbl_empty_message = lv_label_create(macro_container);
    lv_label_set_text(lbl_empty_message, macroNoMacroMessage());
    lv_obj_set_style_text_font(lbl_empty_message, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_empty_message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(lbl_empty_message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_empty_message, UI_SCALE_X(700));  // Constrain width for better text layout
    lv_obj_center(lbl_empty_message);  // Center both horizontally and vertically
    lv_obj_add_flag(lbl_empty_message, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    // Macros already loaded at line 52, just refresh the display
    refreshMacroList();
    updateRecordButtonState();
}

// Load macros from Preferences
void UITabMacros::loadMacros() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);  // Read-only
    
    int machine_index = prefs.getInt("sel_machine", 0);
    
    char key[32];
    snprintf(key, sizeof(key), "m%d_macros", machine_index);
    
    size_t size = prefs.getBytesLength(key);
    if (size == sizeof(macros)) {
        prefs.getBytes(key, macros, size);
        Serial.printf("Loaded %d macros for machine %d\n", getConfiguredMacroCount(), machine_index);
    } else {
        // Initialize with default empty macros
        for (int i = 0; i < MAX_MACROS; i++) {
            macros[i].is_configured = false;
            memset(macros[i].name, 0, sizeof(macros[i].name));
            memset(macros[i].file_path, 0, sizeof(macros[i].file_path));
            macros[i].color_index = i % 8;  // Cycle through colors
        }
        Serial.printf("Initialized empty macros for machine %d\n", machine_index);
    }
    
    prefs.end();
}

// Save macros to Preferences
void UITabMacros::saveMacros() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);  // Read-write
    
    int machine_index = prefs.getInt("sel_machine", 0);
    
    char key[32];
    snprintf(key, sizeof(key), "m%d_macros", machine_index);
    
    prefs.putBytes(key, macros, sizeof(macros));
    Serial.printf("Saved %d macros for machine %d\n", getConfiguredMacroCount(), machine_index);
    
    prefs.end();
}

// Get count of configured macros
int UITabMacros::getConfiguredMacroCount() {
    int count = 0;
    for (int i = 0; i < MAX_MACROS; i++) {
        if (macros[i].is_configured) {
            count++;
        }
    }
    return count;
}

// Get color by index (0-7)
lv_color_t UITabMacros::getColorByIndex(int index) {
    switch (index) {
        case 0: return UITheme::MACRO_COLOR_1;
        case 1: return UITheme::MACRO_COLOR_2;
        case 2: return UITheme::MACRO_COLOR_3;
        case 3: return UITheme::MACRO_COLOR_4;
        case 4: return UITheme::MACRO_COLOR_5;
        case 5: return UITheme::MACRO_COLOR_6;
        case 6: return UITheme::MACRO_COLOR_7;
        case 7: return UITheme::MACRO_COLOR_8;
        default: return UITheme::BTN_CONNECT;
    }
}

// Swap two macros in the array
void UITabMacros::swapMacros(int index1, int index2) {
    if (index1 < 0 || index1 >= MAX_MACROS || index2 < 0 || index2 >= MAX_MACROS) {
        return;
    }
    
    MacroConfig temp = macros[index1];
    macros[index1] = macros[index2];
    macros[index2] = temp;
    
    saveMacros();
    refreshMacroList();
}

// Refresh the macro list display
void UITabMacros::refreshMacroList() {
    // Clear all children from container (prevents duplication)
    lv_obj_clean(macro_container);
    
    // Reset button pointers
    for (int i = 0; i < MAX_MACROS; i++) {
        macro_buttons[i] = nullptr;
        up_buttons[i] = nullptr;
        down_buttons[i] = nullptr;
        edit_buttons[i] = nullptr;
        delete_buttons[i] = nullptr;
    }
    
    if (is_edit_mode) {
        // EDIT MODE: Absolute positioning with control buttons
        lv_obj_set_layout(macro_container, LV_LAYOUT_NONE);
        lv_obj_set_style_pad_all(macro_container, UI_SCALE_Y(5), 0);
        lv_obj_add_flag(macro_container, LV_OBJ_FLAG_SCROLLABLE);  // Enable scrolling in edit mode
        lv_obj_set_scroll_dir(macro_container, LV_DIR_VER);  // Vertical scrolling only
        
        int displayed_index = 0;
        for (int i = 0; i < MAX_MACROS; i++) {
            if (!macros[i].is_configured) continue;
            
            int y_pos = displayed_index * UI_SCALE_Y(65); // 60px button + 5px gap
            
            // Macro button (shows name + color) - increased by 10px to 488px
            // Note: No click event in edit mode - button is just for display
            macro_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(macro_buttons[i], UI_SCALE_X(488), UI_SCALE_Y(60));  // Increased from 478 to 488
            lv_obj_set_pos(macro_buttons[i], 0, y_pos);
            lv_obj_set_style_bg_color(macro_buttons[i], getColorByIndex(macros[i].color_index), 0);
            lv_obj_add_flag(macro_buttons[i], LV_OBJ_FLAG_CLICKABLE);  // Make non-clickable
            
            lv_obj_t *btn_label = lv_label_create(macro_buttons[i]);
            lv_label_set_text(btn_label, macros[i].name);
            lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_22, 0);
            lv_obj_align(btn_label, LV_ALIGN_LEFT_MID, UI_SCALE_X(10), 0);
            
            // Up button
            up_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(up_buttons[i], UI_SCALE_X(60), UI_SCALE_Y(60));
            lv_obj_set_pos(up_buttons[i], UI_SCALE_X(493), y_pos);  // Shifted right by 10px (was 483)
            lv_obj_set_style_bg_color(up_buttons[i], UITheme::BG_BUTTON, 0);
            lv_obj_add_event_cb(up_buttons[i], onMoveUpMacro, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            if (displayed_index == 0) {
                lv_obj_add_state(up_buttons[i], LV_STATE_DISABLED);
            }
            
            lv_obj_t *up_label = lv_label_create(up_buttons[i]);
            lv_label_set_text(up_label, LV_SYMBOL_UP);
            lv_obj_set_style_text_font(up_label, &lv_font_montserrat_22, 0);
            lv_obj_center(up_label);
            
            // Down button
            down_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(down_buttons[i], UI_SCALE_X(60), UI_SCALE_Y(60));
            lv_obj_set_pos(down_buttons[i], UI_SCALE_X(558), y_pos);  // Shifted right by 10px (was 548)
            lv_obj_set_style_bg_color(down_buttons[i], UITheme::BG_BUTTON, 0);
            lv_obj_add_event_cb(down_buttons[i], onMoveDownMacro, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            // Count total configured to disable last item's down button
            int configured_count = 0;
            for (int j = 0; j < MAX_MACROS; j++) {
                if (macros[j].is_configured) configured_count++;
            }
            if (displayed_index == configured_count - 1) {
                lv_obj_add_state(down_buttons[i], LV_STATE_DISABLED);
            }
            
            lv_obj_t *down_label = lv_label_create(down_buttons[i]);
            lv_label_set_text(down_label, LV_SYMBOL_DOWN);
            lv_obj_set_style_text_font(down_label, &lv_font_montserrat_22, 0);
            lv_obj_center(down_label);
            
            // Edit button - same width as ordering buttons
            edit_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(edit_buttons[i], UI_SCALE_X(60), UI_SCALE_Y(60));  // Increased from 55 to 60
            lv_obj_set_pos(edit_buttons[i], UI_SCALE_X(623), y_pos);  // Shifted right by 10px (was 613)
            lv_obj_set_style_bg_color(edit_buttons[i], UITheme::ACCENT_SECONDARY, 0);
            lv_obj_add_event_cb(edit_buttons[i], onEditMacro, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            lv_obj_t *edit_label = lv_label_create(edit_buttons[i]);
            lv_label_set_text(edit_label, LV_SYMBOL_EDIT);
            lv_obj_set_style_text_font(edit_label, &lv_font_montserrat_22, 0);
            lv_obj_center(edit_label);
            
            // Delete button - same width as ordering buttons
            delete_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(delete_buttons[i], UI_SCALE_X(60), UI_SCALE_Y(60));  // Increased from 55 to 60
            lv_obj_set_pos(delete_buttons[i], UI_SCALE_X(688), y_pos);  // Shifted right by 15px (was 673)
            lv_obj_set_style_bg_color(delete_buttons[i], UITheme::STATE_ALARM, 0);
            lv_obj_add_event_cb(delete_buttons[i], onDeleteMacro, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            
            lv_obj_t *delete_label = lv_label_create(delete_buttons[i]);
            lv_label_set_text(delete_label, LV_SYMBOL_TRASH);
            lv_obj_set_style_text_font(delete_label, &lv_font_montserrat_20, 0);
            lv_obj_center(delete_label);
            
            displayed_index++;
        }
    } else {
        // NORMAL MODE: 3x3 flex grid
        lv_obj_clear_flag(macro_container, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling in normal mode
        lv_obj_set_layout(macro_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(macro_container, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(macro_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_all(macro_container, UI_SCALE_Y(10), 0);
        lv_obj_set_style_pad_gap(macro_container, UI_SCALE_X(13), 0);
        
        for (int i = 0; i < MAX_MACROS; i++) {
            if (!macros[i].is_configured) continue;
            
            macro_buttons[i] = lv_btn_create(macro_container);
            lv_obj_set_size(macro_buttons[i], UI_SCALE_X(240), UI_SCALE_Y(73));
            lv_obj_set_style_bg_color(macro_buttons[i], getColorByIndex(macros[i].color_index), 0);
            lv_obj_set_style_pad_all(macro_buttons[i], UI_SCALE_Y(10), 0);
            lv_obj_add_event_cb(macro_buttons[i], onMacroClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
            lv_obj_add_event_cb(macro_buttons[i], onMacroLongPressed, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
            
            lv_obj_t *label = lv_label_create(macro_buttons[i]);
            lv_label_set_text(label, macros[i].name);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_obj_center(label);
        }
    }
    
    // Show/hide empty message based on whether any macros are configured
    int configured_count = getConfiguredMacroCount();
    if (configured_count == 0 && !is_edit_mode) {
        // Clear flex layout and padding for proper centering
        lv_obj_set_layout(macro_container, LV_LAYOUT_NONE);
        lv_obj_set_style_pad_all(macro_container, 0, 0);
        
        // Recreate empty message label since we cleared the container
        lbl_empty_message = lv_label_create(macro_container);
        lv_label_set_text(lbl_empty_message, macroNoMacroMessage());
        lv_obj_set_style_text_font(lbl_empty_message, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_empty_message, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_style_text_align(lbl_empty_message, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lbl_empty_message, UI_SCALE_X(700));  // Constrain width for better text layout
        lv_obj_center(lbl_empty_message);  // Center both horizontally and vertically
    }
}

// Find previous configured macro index
bool UITabMacros::findPreviousConfiguredIndex(int current_index) {
    for (int i = current_index - 1; i >= 0; i--) {
        if (macros[i].is_configured) {
            return true;
        }
    }
    return false;
}

// Find next configured macro index
bool UITabMacros::findNextConfiguredIndex(int current_index) {
    for (int i = current_index + 1; i < MAX_MACROS; i++) {
        if (macros[i].is_configured) {
            return true;
        }
    }
    return false;
}

// Toggle between Normal and Edit modes
void UITabMacros::onEditModeToggle(lv_event_t *e) {
    if (!is_edit_mode && is_recording) {
        Serial.println("[Macros] Stop recording before entering edit mode");
        return;
    }
    is_edit_mode = !is_edit_mode;
    
    if (is_edit_mode) {
        lv_obj_add_flag(btn_edit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_record, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_add, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_done, LV_OBJ_FLAG_HIDDEN);
        hideProgress();  // Hide progress in edit mode
    } else {
        lv_obj_clear_flag(btn_edit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_record, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_add, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_done, LV_OBJ_FLAG_HIDDEN);
    }
    
    updateRecordButtonState();
    refreshMacroList();
}

bool UITabMacros::isLocalMacro(int index) {
#if defined(FT_PLATFORM_PC)
    return index >= 0 && index < MAX_MACROS && macros[index].is_configured;
#else
    (void)index;
    return false;
#endif
}

void UITabMacros::executeMacro(int index, int repeat_count) {
    if (index < 0 || index >= MAX_MACROS || !macros[index].is_configured) {
        return;
    }
    if (repeat_count < 1) {
        repeat_count = 1;
    }

    Serial.printf("Macro execute requested: %s (%s), repeat=%d\n",
                  macros[index].name, macros[index].file_path, repeat_count);

    // Store the macro name for progress display
    strncpy(running_macro_name, macros[index].name, sizeof(running_macro_name) - 1);
    running_macro_name[sizeof(running_macro_name) - 1] = '\0';
    updateRecordButtonState();

    UITabMacros::updateProgress(0, running_macro_name, "Starting...");
    UITabMacros::showProgress();

    char command[320];
    if (repeat_count > 1 && isLocalMacro(index)) {
#if defined(FT_PLATFORM_PC)
        snprintf(command, sizeof(command), "$LocalFS/RunRepeat=%d|%s/%s\n",
                 repeat_count, macroStoragePathCommand(), macros[index].file_path);
#else
        snprintf(command, sizeof(command), "%s%s/%s\n",
                 macroRunCommandPrefix(), macroStoragePathCommand(), macros[index].file_path);
#endif
    } else {
        snprintf(command, sizeof(command), "%s%s/%s\n",
                 macroRunCommandPrefix(), macroStoragePathCommand(), macros[index].file_path);
    }

    Serial.printf("Executing macro command: %s\n", command);
    CommManager::sendCommand(command);
}

// Handle macro button click (execute macro)
void UITabMacros::onMacroClicked(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    if (suppress_next_macro_click) {
        suppress_next_macro_click = false;
        return;
    }
    executeMacro(index, 1);
}

void UITabMacros::onMacroLongPressed(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    if (!isLocalMacro(index) || isMacroRunning() || is_recording) {
        return;
    }
    suppress_next_macro_click = true;
    showRepeatDialog(index);
}

// Show Add Macro dialog
void UITabMacros::onAddMacro(lv_event_t *e) {
    // Find first empty slot
    for (int i = 0; i < MAX_MACROS; i++) {
        if (!macros[i].is_configured) {
            editing_index = i;
            showConfigDialog(true);
            return;
        }
    }
    
    Serial.println("No empty macro slots available");
}

// Refresh file list button handler
void UITabMacros::onRefreshFiles(lv_event_t *e) {
    Serial.println("[Macros] Refresh button clicked");
    loadMacroFilesFromSD();
}

int UITabMacros::findFirstEmptySlot() {
    for (int i = 0; i < MAX_MACROS; i++) {
        if (!macros[i].is_configured) {
            return i;
        }
    }
    return -1;
}

bool UITabMacros::isCommandRecordable(const char *command) {
    if (!command || command[0] == '\0') {
        return false;
    }
    for (const char *p = command; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if ((c < 32 || c > 126) && c != '\r' && c != '\n' && c != '\t') {
            return false;
        }
    }
    return true;
}

std::string UITabMacros::normalizeRecordedCommand(const char *command) {
    if (!command) {
        return std::string();
    }
    std::string normalized(command);
    while (!normalized.empty() &&
           (normalized.back() == '\r' || normalized.back() == '\n')) {
        normalized.pop_back();
    }
    if (normalized.empty()) {
        return std::string();
    }
    normalized.push_back('\n');
    return normalized;
}

std::string UITabMacros::sanitizeFilenameBase(const char *name) {
    std::string out;
    if (!name) {
        return out;
    }
    for (const char *p = name; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == ' ' || c == '-' || c == '_') {
            out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    if (out.empty()) {
        out = "recorded_macro";
    }
    if (out.length() > 24) {
        out.resize(24);
    }
    return out;
}

bool UITabMacros::writeRecordedMacroFile(const char *filename, std::string &local_path_out) {
    if (!filename || filename[0] == '\0') {
        return false;
    }
#if defined(FT_PLATFORM_PC)
    const std::string dir_macros = EnvPlatform::macroLocalBaseDir();
    const size_t macros_sep = dir_macros.find_last_of('/');
    const std::string dir_fluidtouch = (macros_sep == std::string::npos) ? "." : dir_macros.substr(0, macros_sep);
    const size_t fluidtouch_sep = dir_fluidtouch.find_last_of('/');
    const std::string dir_localfs = (fluidtouch_sep == std::string::npos) ? "." : dir_fluidtouch.substr(0, fluidtouch_sep);

    if ((mkdir(dir_localfs.c_str(), 0777) != 0) && errno != EEXIST) {
        Serial.printf("[Macros] Failed to create %s (errno=%d)\n", dir_localfs.c_str(), errno);
        return false;
    }
    if ((mkdir(dir_fluidtouch.c_str(), 0777) != 0) && errno != EEXIST) {
        Serial.printf("[Macros] Failed to create %s (errno=%d)\n", dir_fluidtouch.c_str(), errno);
        return false;
    }
    if ((mkdir(dir_macros.c_str(), 0777) != 0) && errno != EEXIST) {
        Serial.printf("[Macros] Failed to create %s (errno=%d)\n", dir_macros.c_str(), errno);
        return false;
    }

    local_path_out = dir_macros + "/" + filename;
    std::ofstream out(local_path_out.c_str(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        Serial.printf("[Macros] Failed to open local recording file: %s\n", local_path_out.c_str());
        return false;
    }
    for (const std::string &line : recordedCommandsStorage()) {
        out << line;
    }
    out.close();
    return true;
#else
    if (!UploadManager::init()) {
        Serial.println("[Macros] Failed to initialize SD for macro recording");
        return false;
    }

    SD.mkdir("/fluidtouch");
    SD.mkdir("/fluidtouch/macros");
    local_path_out = std::string("/fluidtouch/macros/") + filename;

    if (SD.exists(local_path_out.c_str())) {
        SD.remove(local_path_out.c_str());
    }
    File file = SD.open(local_path_out.c_str(), FILE_WRITE);
    if (!file) {
        Serial.printf("[Macros] Failed to open recording file: %s\n", local_path_out.c_str());
        return false;
    }
    for (const std::string &line : recordedCommandsStorage()) {
        file.print(line.c_str());
    }
    file.close();
    return true;
#endif
}

bool UITabMacros::addRecordedMacroConfig(const char *macro_name, const char *filename) {
    int slot = findFirstEmptySlot();
    if (slot < 0) {
        Serial.println("[Macros] No empty macro slot available for recorded macro");
        return false;
    }
    strncpy(macros[slot].name, macro_name, sizeof(macros[slot].name) - 1);
    macros[slot].name[sizeof(macros[slot].name) - 1] = '\0';
    strncpy(macros[slot].file_path, filename, sizeof(macros[slot].file_path) - 1);
    macros[slot].file_path[sizeof(macros[slot].file_path) - 1] = '\0';
    macros[slot].color_index = slot % 8;
    macros[slot].is_configured = true;
    saveMacros();
    refreshMacroList();
    return true;
}

void UITabMacros::updateRecordButtonState() {
    if (!btn_record) {
        return;
    }
    const bool macro_running = isMacroRunning();
    const bool stop_mode = is_recording || macro_running;
    lv_obj_set_style_bg_color(btn_record,
                              stop_mode ? UITheme::STATE_ALARM : UITheme::ACCENT_PRIMARY,
                              0);
    lv_obj_t *label = lv_obj_get_child(btn_record, 0);
    if (label) {
        lv_label_set_text(label, stop_mode ? "Stop" : "Rec");
    }
}

void UITabMacros::onRecordToggle(lv_event_t *e) {
    (void)e;
    if (!is_recording && isMacroRunning()) {
        Serial.println("[Macros] Stop requested for running macro");
#if defined(FT_PLATFORM_PC)
        CommManager::sendCommand("$LocalFS/Stop\n");
#else
        // Best-effort immediate stop for active SD/streamed macro execution.
        CommManager::sendCommand("!\n");
#endif
        clearRunningMacro();
        hideProgress();
        updateRecordButtonState();
        return;
    }

    if (!is_recording) {
        if (findFirstEmptySlot() < 0) {
            Serial.println("[Macros] Cannot start recording: all macro slots are in use");
            return;
        }
        recordedCommandsStorage().clear();
        is_recording = true;
        CommManager::setCommandTap([](const char *command) {
            if (!UITabMacros::is_recording || !UITabMacros::isCommandRecordable(command)) {
                return;
            }
            std::string normalized = UITabMacros::normalizeRecordedCommand(command);
            if (!normalized.empty()) {
                recordedCommandsStorage().push_back(normalized);
            }
        });
        Serial.println("[Macros] Command recording started");
        updateRecordButtonState();
        return;
    }

    is_recording = false;
    CommManager::clearCommandTap();
    updateRecordButtonState();

    if (recordedCommandsStorage().empty()) {
        Serial.println("[Macros] No recordable commands captured");
        return;
    }
    showRecordSaveDialog();
}

// Show Edit Macro dialog
void UITabMacros::onEditMacro(lv_event_t *e) {
    editing_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (editing_index < 0 || editing_index >= MAX_MACROS) {
        return;
    }
    
    showConfigDialog(false);
}

// Show Delete Confirmation dialog
void UITabMacros::onDeleteMacro(lv_event_t *e) {
    editing_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (editing_index < 0 || editing_index >= MAX_MACROS) {
        return;
    }
    
    showDeleteConfirmDialog();
}

// Move macro up
void UITabMacros::onMoveUpMacro(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Find previous configured macro
    for (int i = index - 1; i >= 0; i--) {
        if (macros[i].is_configured) {
            swapMacros(index, i);
            return;
        }
    }
}

// Move macro down
void UITabMacros::onMoveDownMacro(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Find next configured macro
    for (int i = index + 1; i < MAX_MACROS; i++) {
        if (macros[i].is_configured) {
            swapMacros(index, i);
            return;
        }
    }
}

// Show configuration dialog (Add or Edit)
void UITabMacros::showConfigDialog(bool is_add) {
    // Create modal background
    config_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(config_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(config_dialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(config_dialog, LV_OPA_60, 0);
    lv_obj_set_style_border_width(config_dialog, 0, 0);
    lv_obj_clear_flag(config_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create dialog container
    lv_obj_t *dialog = lv_obj_create(config_dialog);
    lv_obj_set_size(dialog, UI_SCALE_X(600), UI_SCALE_Y(450));
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_border_color(dialog, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_style_pad_all(dialog, UI_SCALE_Y(20), 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, is_add ? "Add Macro" : "Edit Macro");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(title, 0, 0);
    
    // Name label
    lv_obj_t *name_label = lv_label_create(dialog);
    lv_label_set_text(name_label, "Name:");
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(name_label, 0, UI_SCALE_Y(50));
    
    // Name textarea
    config_name_textarea = lv_textarea_create(dialog);
    lv_obj_set_size(config_name_textarea, UI_SCALE_X(560), UI_SCALE_Y(50));
    lv_obj_set_pos(config_name_textarea, 0, UI_SCALE_Y(80));
    lv_obj_set_style_text_font(config_name_textarea, &lv_font_montserrat_20, 0);
    lv_textarea_set_max_length(config_name_textarea, 31);
    lv_textarea_set_one_line(config_name_textarea, true);
    lv_obj_add_event_cb(config_name_textarea, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
    
    if (!is_add) {
        lv_textarea_set_text(config_name_textarea, macros[editing_index].name);
    }
    
    // File Path label
    lv_obj_t *path_label = lv_label_create(dialog);
    lv_label_set_text_fmt(path_label, "File: %s", macroStoragePathDisplay());
    lv_obj_set_style_text_font(path_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(path_label, 0, UI_SCALE_Y(145));
    
    // Load macro files from SD card
    loadMacroFilesFromSD();
    
    // File Path dropdown
    config_path_dropdown = lv_dropdown_create(dialog);
    lv_obj_set_size(config_path_dropdown, UI_SCALE_X(490), UI_SCALE_Y(50));
    lv_obj_set_pos(config_path_dropdown, 0, UI_SCALE_Y(175));
    lv_obj_set_style_text_font(config_path_dropdown, &lv_font_montserrat_20, 0);
    
    // Refresh button for file list
    lv_obj_t *btn_refresh = lv_button_create(dialog);
    lv_obj_set_size(btn_refresh, UI_SCALE_X(50), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_refresh, UI_SCALE_X(510), UI_SCALE_Y(175));
    lv_obj_set_style_bg_color(btn_refresh, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(btn_refresh, onRefreshFiles, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *refresh_icon = lv_label_create(btn_refresh);
    lv_label_set_text(refresh_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(refresh_icon, &lv_font_montserrat_20, 0);
    lv_obj_center(refresh_icon);
    
    // Populate dropdown with files
    if (macroFilesStorage().empty()) {
        lv_dropdown_set_options(config_path_dropdown, "Loading files...");
    } else {
        // Files already loaded, populate dropdown and select current file
        String options = "-- Select a file --";  // Add blank placeholder at top
        int selected_idx = 0;  // Default to placeholder
        
        for (size_t i = 0; i < macroFilesStorage().size(); i++) {
            options += "\n";
            options += macroFilesStorage()[i].c_str();
            
            // Find the currently selected file if editing (add 1 to index for placeholder offset)
            if (!is_add && strcmp(macroFilesStorage()[i].c_str(), macros[editing_index].file_path) == 0) {
                selected_idx = i + 1;  // +1 for placeholder at index 0
            }
        }
        
        lv_dropdown_set_options(config_path_dropdown, options.c_str());
        if (!is_add) {
            lv_dropdown_set_selected(config_path_dropdown, selected_idx);
        }
    }
    
    // Color label
    lv_obj_t *color_label = lv_label_create(dialog);
    lv_label_set_text(color_label, "Color:");
    lv_obj_set_style_text_font(color_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(color_label, 0, UI_SCALE_Y(240));
    
    // Color button grid (single row of 8 buttons)
    int initial_color = is_add ? 0 : macros[editing_index].color_index;
    selected_color_index = initial_color;
    
    for (int i = 0; i < 8; i++) {
        config_color_buttons[i] = lv_btn_create(dialog);
        lv_obj_set_size(config_color_buttons[i], UI_SCALE_X(60), UI_SCALE_Y(60));
        lv_obj_set_pos(config_color_buttons[i], UI_SCALE_X(i * 70), UI_SCALE_Y(270));
        lv_obj_set_style_bg_color(config_color_buttons[i], getColorByIndex(i), 0);
        lv_obj_set_style_radius(config_color_buttons[i], 8, 0);
        lv_obj_add_event_cb(config_color_buttons[i], onColorButtonClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        // Add checkmark to selected color
        if (i == initial_color) {
            lv_obj_set_style_border_width(config_color_buttons[i], 3, 0);
            lv_obj_set_style_border_color(config_color_buttons[i], lv_color_hex(0xFFFFFF), 0);
            
            lv_obj_t *check = lv_label_create(config_color_buttons[i]);
            lv_label_set_text(check, LV_SYMBOL_OK);
            lv_obj_set_style_text_font(check, &lv_font_montserrat_24, 0);
            lv_obj_set_style_text_color(check, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(check);
        } else {
            lv_obj_set_style_border_width(config_color_buttons[i], 1, 0);
            lv_obj_set_style_border_color(config_color_buttons[i], UITheme::BORDER_MEDIUM, 0);
        }
    }
    
    // Cancel button
    // Cancel button (centered: dialog width 600 - 20 padding * 2 = 560 usable, centered pair of 130px buttons with 20px gap)
    lv_obj_t *btn_cancel = lv_btn_create(dialog);
    lv_obj_set_size(btn_cancel, UI_SCALE_X(130), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_cancel, UI_SCALE_X(175), UI_SCALE_Y(360));
    lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_MEDIUM, 0);
    lv_obj_add_event_cb(btn_cancel, onConfigCancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);
    
    // Save button
    lv_obj_t *btn_save = lv_btn_create(dialog);
    lv_obj_set_size(btn_save, UI_SCALE_X(130), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_save, UI_SCALE_X(325), UI_SCALE_Y(360));
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(btn_save, onConfigSave, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *save_label = lv_label_create(btn_save);
    lv_label_set_text(save_label, "Save");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_18, 0);
    lv_obj_center(save_label);
}

// Hide configuration dialog
void UITabMacros::hideConfigDialog() {
    if (keyboard != nullptr) {
        hideKeyboard();
    }
    
    if (config_dialog != nullptr) {
        lv_obj_del(config_dialog);
        config_dialog = nullptr;
        config_name_textarea = nullptr;
        config_path_dropdown = nullptr;
        for (int i = 0; i < 8; i++) {
            config_color_buttons[i] = nullptr;
        }
    }
}

// Save configuration from dialog
void UITabMacros::onConfigSave(lv_event_t *e) {
    if (editing_index < 0 || editing_index >= MAX_MACROS) {
        return;
    }
    
    const char *name = lv_textarea_get_text(config_name_textarea);
    int color_index = selected_color_index;
    
    // Get selected file from dropdown
    char path_buffer[128];
    lv_dropdown_get_selected_str(config_path_dropdown, path_buffer, sizeof(path_buffer));
    
    // Validate inputs
    if (strlen(name) == 0) {
        Serial.println("Macro name is required");
        return;
    }
    
    if (strlen(path_buffer) == 0 || strcmp(path_buffer, "Loading files...") == 0 || 
        strcmp(path_buffer, "-- Select a file --") == 0) {
        Serial.println("Macro file path is required");
        return;
    }
    
    // Save macro configuration
    strncpy(macros[editing_index].name, name, sizeof(macros[editing_index].name) - 1);
    strncpy(macros[editing_index].file_path, path_buffer, sizeof(macros[editing_index].file_path) - 1);
    macros[editing_index].color_index = color_index;
    macros[editing_index].is_configured = true;
    
    saveMacros();
    hideConfigDialog();
    refreshMacroList();
}

// Cancel configuration dialog
void UITabMacros::onConfigCancel(lv_event_t *e) {
    hideConfigDialog();
}

// Show delete confirmation dialog
void UITabMacros::showDeleteConfirmDialog() {
    // Create modal background
    delete_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(delete_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(delete_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(delete_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(delete_dialog, 0, 0);
    lv_obj_clear_flag(delete_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(delete_dialog);
    lv_obj_set_size(content, UI_SCALE_X(500), UI_SCALE_Y(220));
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::STATE_ALARM, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(content, UI_SCALE_Y(20), 0);
    lv_obj_set_style_pad_gap(content, UI_SCALE_Y(15), 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Warning icon and title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text_fmt(title, "%s Delete Macro?", LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
    
    // Macro name
    lv_obj_t *name_label = lv_label_create(content);
    lv_label_set_text(name_label, macros[editing_index].name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_label, UI_SCALE_X(450));
    
    // Message
    lv_obj_t *msg_label = lv_label_create(content);
    lv_label_set_text(msg_label, "This action cannot be undone.");
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(msg_label, UITheme::UI_WARNING, 0);
    
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
    lv_obj_set_size(cancel_btn, UI_SCALE_X(180), UI_SCALE_Y(50));
    lv_obj_set_style_bg_color(cancel_btn, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(cancel_btn, onDeleteCancel, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);
    
    // Delete button
    lv_obj_t *delete_btn = lv_btn_create(btn_container);
    lv_obj_set_size(delete_btn, UI_SCALE_X(180), UI_SCALE_Y(50));
    lv_obj_set_style_bg_color(delete_btn, UITheme::STATE_ALARM, 0);
    lv_obj_add_event_cb(delete_btn, onDeleteConfirm, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *delete_label = lv_label_create(delete_btn);
    lv_label_set_text(delete_label, LV_SYMBOL_TRASH " Delete");
    lv_obj_set_style_text_font(delete_label, &lv_font_montserrat_18, 0);
    lv_obj_center(delete_label);
}

// Hide delete confirmation dialog
void UITabMacros::hideDeleteConfirmDialog() {
    if (delete_dialog != nullptr) {
        lv_obj_del(delete_dialog);
        delete_dialog = nullptr;
    }
}

// Confirm macro deletion
void UITabMacros::onDeleteConfirm(lv_event_t *e) {
    if (editing_index < 0 || editing_index >= MAX_MACROS) {
        return;
    }
    
    // Clear macro configuration
    macros[editing_index].is_configured = false;
    memset(macros[editing_index].name, 0, sizeof(macros[editing_index].name));
    memset(macros[editing_index].file_path, 0, sizeof(macros[editing_index].file_path));
    
    saveMacros();
    hideDeleteConfirmDialog();
    refreshMacroList();
}

// Cancel macro deletion
void UITabMacros::onDeleteCancel(lv_event_t *e) {
    hideDeleteConfirmDialog();
}

void UITabMacros::showRecordSaveDialog() {
    hideRecordSaveDialog();

    record_save_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(record_save_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(record_save_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(record_save_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(record_save_dialog, 0, 0);
    lv_obj_clear_flag(record_save_dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dialog = lv_obj_create(record_save_dialog);
    lv_obj_set_size(dialog, UI_SCALE_X(560), UI_SCALE_Y(300));
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_border_color(dialog, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_style_pad_all(dialog, UI_SCALE_Y(20), 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "Save Recorded Macro");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_pos(title, 0, 0);

    lv_obj_t *desc = lv_label_create(dialog);
    lv_label_set_text_fmt(desc, "Captured commands: %d", (int)recordedCommandsStorage().size());
    lv_obj_set_style_text_font(desc, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(desc, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(desc, 0, UI_SCALE_Y(45));

    lv_obj_t *name_label = lv_label_create(dialog);
    lv_label_set_text(name_label, "Macro Name:");
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(name_label, 0, UI_SCALE_Y(85));

    record_name_textarea = lv_textarea_create(dialog);
    lv_obj_set_size(record_name_textarea, UI_SCALE_X(520), UI_SCALE_Y(52));
    lv_obj_set_pos(record_name_textarea, 0, UI_SCALE_Y(120));
    lv_obj_set_style_text_font(record_name_textarea, &lv_font_montserrat_20, 0);
    lv_textarea_set_max_length(record_name_textarea, 31);
    lv_textarea_set_one_line(record_name_textarea, true);
    lv_textarea_set_text(record_name_textarea, "Recorded Macro");
    lv_obj_add_event_cb(record_name_textarea, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);

    lv_obj_t *btn_cancel = lv_btn_create(dialog);
    lv_obj_set_size(btn_cancel, UI_SCALE_X(170), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_cancel, UI_SCALE_X(90), UI_SCALE_Y(220));
    lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_MEDIUM, 0);
    lv_obj_add_event_cb(btn_cancel, onRecordSaveCancel, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_label, "Discard");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);

    lv_obj_t *btn_save = lv_btn_create(dialog);
    lv_obj_set_size(btn_save, UI_SCALE_X(170), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_save, UI_SCALE_X(300), UI_SCALE_Y(220));
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(btn_save, onRecordSaveConfirm, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *save_label = lv_label_create(btn_save);
    lv_label_set_text(save_label, "Save");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_18, 0);
    lv_obj_center(save_label);
}

void UITabMacros::hideRecordSaveDialog() {
    if (record_save_dialog != nullptr) {
        if (keyboard != nullptr) {
            hideKeyboard();
        }
        lv_obj_del(record_save_dialog);
        record_save_dialog = nullptr;
        record_name_textarea = nullptr;
    }
}

void UITabMacros::onRecordSaveCancel(lv_event_t *e) {
    (void)e;
    recordedCommandsStorage().clear();
    hideRecordSaveDialog();
}

void UITabMacros::onRecordSaveConfirm(lv_event_t *e) {
    (void)e;
    if (recordedCommandsStorage().empty()) {
        hideRecordSaveDialog();
        return;
    }

    if (findFirstEmptySlot() < 0) {
        Serial.println("[Macros] Cannot save recording: all macro slots are in use");
        hideRecordSaveDialog();
        recordedCommandsStorage().clear();
        return;
    }

    const char *macro_name = record_name_textarea ? lv_textarea_get_text(record_name_textarea) : "";
    if (!macro_name || macro_name[0] == '\0') {
        Serial.println("[Macros] Recording name is required");
        return;
    }

    const std::string base = sanitizeFilenameBase(macro_name);
    char filename[80];
    snprintf(filename, sizeof(filename), "%s_%lu.gcode", base.c_str(), (unsigned long)millis());

    std::string local_path;
    if (!writeRecordedMacroFile(filename, local_path)) {
        Serial.println("[Macros] Failed to write recorded macro file");
        hideRecordSaveDialog();
        recordedCommandsStorage().clear();
        return;
    }

#if defined(FT_PLATFORM_PC)
    if (!addRecordedMacroConfig(macro_name, filename)) {
        Serial.println("[Macros] Recorded file saved but macro config could not be added");
        hideRecordSaveDialog();
        recordedCommandsStorage().clear();
        return;
    }
#else
    bool upload_ok = false;
    const char *upload_error = nullptr;
    char remote_dir[96];
    snprintf(remote_dir, sizeof(remote_dir), "%s/", macroStoragePathCommand());
    UploadManager::uploadFile(
        local_path.c_str(),
        filename,
        nullptr,
        [&](bool success, const char *error) {
            upload_ok = success;
            upload_error = error;
        },
        remote_dir);

    if (!upload_ok) {
        Serial.printf("[Macros] Failed to upload recorded macro: %s\n", upload_error ? upload_error : "unknown");
        hideRecordSaveDialog();
        recordedCommandsStorage().clear();
        return;
    }

    if (!addRecordedMacroConfig(macro_name, filename)) {
        Serial.println("[Macros] Recorded file uploaded but macro config could not be added");
        hideRecordSaveDialog();
        recordedCommandsStorage().clear();
        return;
    }
#endif

    Serial.printf("[Macros] Recorded macro saved as %s\n", filename);
    hideRecordSaveDialog();
    recordedCommandsStorage().clear();
}

void UITabMacros::showRepeatDialog(int index) {
    if (index < 0 || index >= MAX_MACROS || !macros[index].is_configured) {
        return;
    }
    hideRepeatDialog();
    repeat_macro_index = index;

    repeat_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(repeat_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(repeat_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(repeat_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(repeat_dialog, 0, 0);
    lv_obj_clear_flag(repeat_dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dialog = lv_obj_create(repeat_dialog);
    lv_obj_set_size(dialog, UI_SCALE_X(520), UI_SCALE_Y(300));
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);
    lv_obj_set_style_border_color(dialog, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_style_pad_all(dialog, UI_SCALE_Y(20), 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, LV_SYMBOL_LOOP " Macro Repeat");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *desc = lv_label_create(dialog);
    lv_label_set_text_fmt(desc, "Run \"%s\" how many times?", macros[index].name);
    lv_obj_set_style_text_font(desc, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(desc, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(desc, UI_SCALE_X(460));
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, UI_SCALE_Y(55));

    repeat_count_textarea = lv_textarea_create(dialog);
    lv_obj_set_size(repeat_count_textarea, UI_SCALE_X(120), UI_SCALE_Y(52));
    lv_obj_align(repeat_count_textarea, LV_ALIGN_TOP_MID, 0, UI_SCALE_Y(130));
    lv_obj_set_style_text_font(repeat_count_textarea, &lv_font_montserrat_24, 0);
    lv_textarea_set_text(repeat_count_textarea, "2");
    lv_textarea_set_max_length(repeat_count_textarea, 4);
    lv_textarea_set_one_line(repeat_count_textarea, true);
    lv_obj_add_event_cb(repeat_count_textarea, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);

    lv_obj_t *btn_cancel = lv_btn_create(dialog);
    lv_obj_set_size(btn_cancel, UI_SCALE_X(140), UI_SCALE_Y(50));
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_MID, -UI_SCALE_X(85), 0);
    lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_cancel, onRepeatCancel, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
    lv_obj_center(cancel_label);

    lv_obj_t *btn_run = lv_btn_create(dialog);
    lv_obj_set_size(btn_run, UI_SCALE_X(140), UI_SCALE_Y(50));
    lv_obj_align(btn_run, LV_ALIGN_BOTTOM_MID, UI_SCALE_X(85), 0);
    lv_obj_set_style_bg_color(btn_run, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(btn_run, onRepeatConfirm, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *run_label = lv_label_create(btn_run);
    lv_label_set_text(run_label, "Run");
    lv_obj_set_style_text_font(run_label, &lv_font_montserrat_18, 0);
    lv_obj_center(run_label);
}

void UITabMacros::hideRepeatDialog() {
    if (keyboard != nullptr) {
        hideKeyboard();
    }
    if (repeat_dialog != nullptr) {
        lv_obj_del(repeat_dialog);
        repeat_dialog = nullptr;
        repeat_count_textarea = nullptr;
    }
    repeat_macro_index = -1;
}

void UITabMacros::onRepeatConfirm(lv_event_t *e) {
    (void)e;
    if (repeat_macro_index < 0 || repeat_count_textarea == nullptr) {
        hideRepeatDialog();
        return;
    }
    const char *text = lv_textarea_get_text(repeat_count_textarea);
    int repeat_count = atoi(text ? text : "1");
    if (repeat_count < 1) {
        repeat_count = 1;
    } else if (repeat_count > 999) {
        repeat_count = 999;
    }
    int index = repeat_macro_index;
    hideRepeatDialog();
    executeMacro(index, repeat_count);
}

void UITabMacros::onRepeatCancel(lv_event_t *e) {
    (void)e;
    hideRepeatDialog();
}

// Handle color button clicked
void UITabMacros::onColorButtonClicked(lv_event_t *e) {
    int color_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Update selection
    selected_color_index = color_index;
    
    // Update all button styles
    for (int i = 0; i < 8; i++) {
        if (config_color_buttons[i] != nullptr) {
            // Remove any existing checkmark label
            if (lv_obj_get_child_count(config_color_buttons[i]) > 0) {
                lv_obj_clean(config_color_buttons[i]);
            }
            
            if (i == color_index) {
                // Selected: white border and checkmark
                lv_obj_set_style_border_width(config_color_buttons[i], 3, 0);
                lv_obj_set_style_border_color(config_color_buttons[i], lv_color_hex(0xFFFFFF), 0);
                
                lv_obj_t *check = lv_label_create(config_color_buttons[i]);
                lv_label_set_text(check, LV_SYMBOL_OK);
                lv_obj_set_style_text_font(check, &lv_font_montserrat_24, 0);
                lv_obj_set_style_text_color(check, lv_color_hex(0xFFFFFF), 0);
                lv_obj_center(check);
            } else {
                // Unselected: thin gray border
                lv_obj_set_style_border_width(config_color_buttons[i], 1, 0);
                lv_obj_set_style_border_color(config_color_buttons[i], UITheme::BORDER_MEDIUM, 0);
            }
        }
    }
}

// Handle textarea focus (show keyboard)
void UITabMacros::onTextareaFocused(lv_event_t *e) {
    lv_obj_t *textarea = (lv_obj_t*)lv_event_get_target(e);
    showKeyboard(textarea);
}

// Show keyboard
void UITabMacros::showKeyboard(lv_obj_t *textarea) {
    if (keyboard != nullptr) {
        lv_keyboard_set_textarea(keyboard, textarea);
        return;
    }
    
    keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_set_size(keyboard, SCREEN_WIDTH, UI_SCALE_Y(280));
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);  // Larger font for better visibility
    lv_keyboard_set_textarea(keyboard, textarea);
    
    // Add event handler for keyboard close button
    lv_obj_add_event_cb(keyboard, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            UITabMacros::hideKeyboard();
        }
    }, LV_EVENT_ALL, nullptr);
}

// Hide keyboard
void UITabMacros::hideKeyboard() {
    if (keyboard != nullptr) {
        lv_obj_del(keyboard);
        keyboard = nullptr;
    }
}

// Load macro files from SD card
void UITabMacros::loadMacroFilesFromSD() {
    macroFilesStorage().clear();

#if defined(FT_PLATFORM_PC)
    Serial.println("[Macros] Loading macro files from local filesystem");
    const std::string macros_dir = EnvPlatform::macroLocalBaseDir();
    DIR *dir = opendir(macros_dir.c_str());
    if (dir) {
        struct dirent *entry = nullptr;
        while ((entry = readdir(dir)) != nullptr) {
            const char *name = entry->d_name;
            if (!name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                continue;
            }
#ifdef DT_REG
            if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) {
                continue;
            }
#endif
            macroFilesStorage().emplace_back(name);
        }
        closedir(dir);
    }
    std::sort(macroFilesStorage().begin(), macroFilesStorage().end(),
        [](const std::string &a, const std::string &b) {
            std::string a_lower = a;
            std::string b_lower = b;
            std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
            std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
            return a_lower < b_lower;
        });

    if (UITabMacros::config_path_dropdown) {
        String options = "-- Select a file --";
        int selected_idx = 0;
        for (size_t i = 0; i < macroFilesStorage().size(); i++) {
            options += "\n";
            options += macroFilesStorage()[i].c_str();
            if (UITabMacros::editing_index >= 0 &&
                strcmp(macroFilesStorage()[i].c_str(),
                       UITabMacros::macros[UITabMacros::editing_index].file_path) == 0) {
                selected_idx = i + 1;
            }
        }
        lv_dropdown_set_options(UITabMacros::config_path_dropdown, options.c_str());
        if (UITabMacros::editing_index >= 0) {
            lv_dropdown_set_selected(UITabMacros::config_path_dropdown, selected_idx);
        }
    }
    return;
#endif

    Serial.println("[Macros] Requesting file list from machine storage");
    
    // Register callback to receive JSON file list response
    CommManager::setMessageCallback([](const char* message) {
        static String jsonBuffer;
        static bool collecting = false;
        static uint32_t lastMessageTime = 0;
        
        String msg(message);
        msg.trim();
        uint32_t now = millis();
        
        // Reset buffer if timeout
        if (now - lastMessageTime > 3000) {
            if (jsonBuffer.length() > 0 && collecting) {
                Serial.println("[Macros] Timeout - parsing JSON buffer");
                // Parse the accumulated JSON
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, jsonBuffer);
                
                if (!error && doc["files"].is<JsonArray>()) {
                    JsonArray files = doc["files"];
                    for (JsonObject file : files) {
                        if (file["name"].is<const char*>()) {
                            const char* filename = file["name"];
                            macroFilesStorage().push_back(filename);
                        }
                    }
                    Serial.printf("[Macros] Found %d macro files\n", macroFilesStorage().size());
                    
                    // Sort files alphabetically (case-insensitive)
                    std::sort(macroFilesStorage().begin(), macroFilesStorage().end(),
                        [](const std::string &a, const std::string &b) {
                            std::string a_lower = a;
                            std::string b_lower = b;
                            std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
                            std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
                            return a_lower < b_lower;
                        });
                    
                    // Update dropdown if it exists
                    if (UITabMacros::config_path_dropdown) {
                        String options = "-- Select a file --";  // Add blank placeholder at top
                        int selected_idx = 0;  // Default to placeholder
                        
                        for (size_t i = 0; i < macroFilesStorage().size(); i++) {
                            options += "\n";
                            options += macroFilesStorage()[i].c_str();
                            
                            // Find matching file if editing (add 1 to index for placeholder offset)
                            if (UITabMacros::editing_index >= 0 && 
                                strcmp(macroFilesStorage()[i].c_str(), 
                                       UITabMacros::macros[UITabMacros::editing_index].file_path) == 0) {
                                selected_idx = i + 1;  // +1 for placeholder at index 0
                            }
                        }
                        
                        lv_dropdown_set_options(UITabMacros::config_path_dropdown, options.c_str());
                        
                        // Set selected index if editing
                        if (UITabMacros::editing_index >= 0) {
                            lv_dropdown_set_selected(UITabMacros::config_path_dropdown, selected_idx);
                            Serial.printf("[Macros] Dropdown updated, selected index %d for file '%s'\n", 
                                selected_idx, UITabMacros::macros[UITabMacros::editing_index].file_path);
                        } else {
                            Serial.println("[Macros] Dropdown updated with file list");
                        }
                    }
                }
                CommManager::clearMessageCallback();
            }
            jsonBuffer = "";
            collecting = false;
        }
        lastMessageTime = now;
        
        // Skip status reports and other messages
        if (msg.startsWith("<") || msg.startsWith("[GC:") || msg.startsWith("[MSG:") || msg.startsWith("PING:")) {
            return;
        }
        
        // Check for end of response
        if (msg.equalsIgnoreCase("ok")) {
            if (collecting) {
                Serial.println("[Macros] Received 'ok', parsing JSON");
                // Parse the accumulated JSON
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, jsonBuffer);
                
                if (!error && doc["files"].is<JsonArray>()) {
                    JsonArray files = doc["files"];
                    for (JsonObject file : files) {
                        if (file["name"].is<const char*>()) {
                            const char* filename = file["name"];
                            macroFilesStorage().push_back(filename);
                        }
                    }
                    Serial.printf("[Macros] Found %d macro files\n", macroFilesStorage().size());
                    
                    // Sort files alphabetically (case-insensitive)
                    std::sort(macroFilesStorage().begin(), macroFilesStorage().end(),
                        [](const std::string &a, const std::string &b) {
                            std::string a_lower = a;
                            std::string b_lower = b;
                            std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
                            std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
                            return a_lower < b_lower;
                        });
                    
                    // Update dropdown if it exists
                    if (UITabMacros::config_path_dropdown) {
                        String options = "-- Select a file --";  // Add blank placeholder at top
                        int selected_idx = 0;  // Default to placeholder
                        
                        for (size_t i = 0; i < macroFilesStorage().size(); i++) {
                            options += "\n";
                            options += macroFilesStorage()[i].c_str();
                            
                            // Find matching file if editing (add 1 to index for placeholder offset)
                            if (UITabMacros::editing_index >= 0 && 
                                strcmp(macroFilesStorage()[i].c_str(), 
                                       UITabMacros::macros[UITabMacros::editing_index].file_path) == 0) {
                                selected_idx = i + 1;  // +1 for placeholder at index 0
                            }
                        }
                        
                        lv_dropdown_set_options(UITabMacros::config_path_dropdown, options.c_str());
                        
                        // Set selected index if editing
                        if (UITabMacros::editing_index >= 0) {
                            lv_dropdown_set_selected(UITabMacros::config_path_dropdown, selected_idx);
                            Serial.printf("[Macros] Dropdown updated, selected index %d for file '%s'\n", 
                                selected_idx, UITabMacros::macros[UITabMacros::editing_index].file_path);
                        } else {
                            Serial.println("[Macros] Dropdown updated with file list");
                        }
                    }
                }
                jsonBuffer = "";
                collecting = false;
                CommManager::clearMessageCallback();
            }
            return;
        }
        
        // Start collecting JSON
        if (msg.startsWith("[JSON:") || msg.startsWith("{\"files")) {
            if (!collecting) {
                Serial.println("[Macros] Starting JSON collection");
                collecting = true;
                jsonBuffer = "";
            }
            String jsonLine = msg;
            if (jsonLine.startsWith("[JSON:")) {
                jsonLine.replace("[JSON:", "");
                if (jsonLine.endsWith("]")) {
                    jsonLine.remove(jsonLine.length() - 1);
                }
            }
            jsonBuffer += jsonLine;
        } else if (collecting) {
            // Continue accumulating JSON lines
            jsonBuffer += msg;
        }
    });
    
    // Send command to list files from macros directory
    char command[128];
    snprintf(command, sizeof(command), "$Files/ListGcode=%s\n", macroStoragePathCommand());
    CommManager::sendCommand(command);
}

// Update progress display
void UITabMacros::updateProgress(int percent, const char* macro_name, const char* message) {
    if (!progress_container || !bar_progress || !lbl_percent || !lbl_macro_name || !lbl_message) return;
    
    // Update progress bar
    lv_bar_set_value(bar_progress, percent, LV_ANIM_OFF);
    
    // Update percentage label
    char percent_text[8];
    snprintf(percent_text, sizeof(percent_text), "%d%%", percent);
    lv_label_set_text(lbl_percent, percent_text);
    
    // Update macro name - use stored name instead of filename
    if (running_macro_name[0] != '\0') {
        char name_text[64];
        snprintf(name_text, sizeof(name_text), "Running: %s", running_macro_name);
        lv_label_set_text(lbl_macro_name, name_text);
    }
    
    // Update message
    if (message && strlen(message) > 0) {
        lv_label_set_text(lbl_message, message);
    }
}

// Show progress display
void UITabMacros::showProgress() {
    if (progress_container && !is_edit_mode) {
        lv_obj_clear_flag(progress_container, LV_OBJ_FLAG_HIDDEN);
    }
}

// Hide progress display
void UITabMacros::hideProgress() {
    if (progress_container) {
        lv_obj_add_flag(progress_container, LV_OBJ_FLAG_HIDDEN);
        // Don't clear running_macro_name here - it needs to persist to track if a macro is running
        // It will be cleared when the SD print actually finishes (in main loop)
    }
}

// Check if a macro from this tab is currently running
bool UITabMacros::isMacroRunning() {
    bool is_running = running_macro_name[0] != '\0';
    if (is_running) {
        Serial.printf("[Macros] isMacroRunning() = TRUE, name='%s'\n", running_macro_name);
    }
    return is_running;
}

// Clear running macro tracking (called when SD print finishes)
void UITabMacros::clearRunningMacro() {
    Serial.printf("[Macros] Clearing running macro: '%s'\n", running_macro_name);
    running_macro_name[0] = '\0';
    updateRecordButtonState();
}

// Get the name of the currently running macro
const char* UITabMacros::getRunningMacroName() {
    return running_macro_name;
}
