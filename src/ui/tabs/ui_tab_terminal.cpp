#include "ui/tabs/ui_tab_terminal.h"
#include "ui/ui_theme.h"
#include "fluidnc_client.h"
#include "config.h"

// Static member initialization
lv_obj_t *UITabTerminal::terminal_text = nullptr;
lv_obj_t *UITabTerminal::terminal_cont = nullptr;
lv_obj_t *UITabTerminal::input_field = nullptr;
lv_obj_t *UITabTerminal::keyboard = nullptr;
lv_obj_t *UITabTerminal::auto_scroll_switch = nullptr;
String UITabTerminal::terminal_buffer = "";
bool UITabTerminal::auto_scroll_enabled = true;
bool UITabTerminal::buffer_dirty = false;
uint32_t UITabTerminal::last_update_ms = 0;

void UITabTerminal::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Calculate available height for content
    // Tab content height = SCREEN_HEIGHT (480) - STATUS_BAR_HEIGHT (60) - TAB_BUTTON_HEIGHT (50) = 370px
    const int content_height = SCREEN_HEIGHT - STATUS_BAR_HEIGHT - TAB_BUTTON_HEIGHT;
    const int input_height = 45;
    const int button_width = 100;
    const int margin = 10;
    const int terminal_height = content_height - input_height - (margin * 3);

    // Input text area
    input_field = lv_textarea_create(tab);
    lv_obj_set_size(input_field, SCREEN_WIDTH - (margin * 5) - button_width - 120, input_height);
    lv_obj_set_pos(input_field, 0, 0);
    lv_textarea_set_one_line(input_field, true);
    lv_textarea_set_placeholder_text(input_field, "Enter command...");
    lv_obj_set_style_text_font(input_field, &lv_font_montserrat_18, 0);
    lv_obj_set_style_bg_color(input_field, UITheme::BG_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_text_color(input_field, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(input_field, input_field_event_cb, LV_EVENT_CLICKED, nullptr);
    
    // Auto-scroll toggle (switch + label)
    lv_obj_t *auto_scroll_label = lv_label_create(tab);
    lv_label_set_text(auto_scroll_label, "Auto");
    lv_obj_set_style_text_font(auto_scroll_label, &lv_font_montserrat_14, 0);
    lv_obj_align(auto_scroll_label, LV_ALIGN_TOP_LEFT, SCREEN_WIDTH - button_width - 125, 5);
    
    auto_scroll_switch = lv_switch_create(tab);
    lv_obj_set_size(auto_scroll_switch, 50, 25);
    lv_obj_align(auto_scroll_switch, LV_ALIGN_TOP_LEFT, SCREEN_WIDTH - button_width - 125, 20);
    lv_obj_add_state(auto_scroll_switch, LV_STATE_CHECKED);  // Start enabled
    lv_obj_add_event_cb(auto_scroll_switch, auto_scroll_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    
    // Send button
    lv_obj_t *send_btn = lv_button_create(tab);
    lv_obj_set_size(send_btn, button_width - 10, input_height);
    lv_obj_set_pos(send_btn, SCREEN_WIDTH - button_width - (margin * 3), 0);
    lv_obj_set_style_bg_color(send_btn, UITheme::ACCENT_PRIMARY, LV_PART_MAIN);
    lv_obj_add_event_cb(send_btn, send_button_event_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "Send");
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_18, 0);
    lv_obj_center(send_label);
    
    // Terminal output container
    terminal_cont = lv_obj_create(tab);
    lv_obj_set_size(terminal_cont, SCREEN_WIDTH - (margin * 4), terminal_height - (margin * 2));
    lv_obj_set_pos(terminal_cont, 3, input_height + (margin * 2));
    lv_obj_set_style_bg_color(terminal_cont, UITheme::BG_BLACK, LV_PART_MAIN);
    lv_obj_set_style_border_color(terminal_cont, UITheme::BORDER_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(terminal_cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(terminal_cont, 5, LV_PART_MAIN);
    // Enable scrolling for terminal output
    lv_obj_set_scroll_dir(terminal_cont, LV_DIR_VER);
    
    terminal_text = lv_label_create(terminal_cont);
    lv_label_set_text(terminal_text, "FluidTouch Terminal - waiting for messages...");
    lv_obj_set_style_text_font(terminal_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(terminal_text, UITheme::UI_SUCCESS, 0);
    lv_label_set_long_mode(terminal_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(terminal_text, SCREEN_WIDTH - (margin * 4) - 10);
    
    // Initialize buffer
    terminal_buffer = "FluidTouch Terminal - waiting for messages...\n";
}

// Send button event handler
void UITabTerminal::send_button_event_cb(lv_event_t *e) {
    send_command();
}

// Input field event handler - show keyboard when clicked
void UITabTerminal::input_field_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        // Create keyboard if it doesn't exist
        if (keyboard == nullptr) {
            keyboard = lv_keyboard_create(lv_screen_active());
            lv_keyboard_set_textarea(keyboard, input_field);
            lv_obj_set_size(keyboard, SCREEN_WIDTH, 240);
            lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_READY, nullptr);
            lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_CANCEL, nullptr);
        } else {
            // Show existing keyboard
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Keyboard event handler
void UITabTerminal::keyboard_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_READY) {
        // User pressed "Enter" or "OK"
        send_command();
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_CANCEL) {
        // User pressed "Cancel"
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

// Auto-scroll event handler
void UITabTerminal::auto_scroll_event_cb(lv_event_t *e) {
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    auto_scroll_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    Serial.printf("[Terminal] Auto-scroll %s\n", auto_scroll_enabled ? "enabled" : "disabled");
}

// Send command function
void UITabTerminal::send_command() {
    const char *cmd = lv_textarea_get_text(input_field);
    
    if (cmd != nullptr && strlen(cmd) > 0) {
        // Log to serial for debugging
        Serial.print("[Terminal] Sending command: ");
        Serial.println(cmd);
        
        // Send command to FluidNC with newline
        String cmd_with_newline = String(cmd) + "\n";
        FluidNCClient::sendCommand(cmd_with_newline.c_str());
        
        // Echo command to terminal
        terminal_buffer += "> ";
        terminal_buffer += cmd;
        terminal_buffer += "\n";
        trimBuffer();
        
        // Update display immediately for user commands
        updateDisplay();
        last_update_ms = millis();  // Reset timer
        
        // Clear input field
        lv_textarea_set_text(input_field, "");
    }
    
    // Hide keyboard after sending
    if (keyboard != nullptr) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void UITabTerminal::appendMessage(const char *message) {
    if (message == nullptr || strlen(message) == 0) return;
    
    // Append message to buffer
    terminal_buffer += message;
    if (message[strlen(message) - 1] != '\n') {
        terminal_buffer += "\n";
    }
    
    // Trim buffer if needed
    trimBuffer();
    
    // Mark buffer as dirty - UI will be updated in update() method
    buffer_dirty = true;
}

void UITabTerminal::trimBuffer() {
    // If buffer exceeds max size, remove lines from the beginning
    if (terminal_buffer.length() > MAX_BUFFER_SIZE) {
        // Find position to start trimming (keep last 75% of buffer)
        size_t trim_to = MAX_BUFFER_SIZE * 3 / 4;
        
        // Find the first newline after the trim point to avoid cutting mid-line
        int newline_pos = terminal_buffer.indexOf('\n', terminal_buffer.length() - trim_to);
        
        if (newline_pos > 0) {
            terminal_buffer = terminal_buffer.substring(newline_pos + 1);
        } else {
            // No newline found, just trim to size
            terminal_buffer = terminal_buffer.substring(terminal_buffer.length() - trim_to);
        }
        
        Serial.printf("[Terminal] Buffer trimmed to %d bytes\n", terminal_buffer.length());
    }
}

void UITabTerminal::update() {
    // Check if enough time has passed since last update
    uint32_t now = millis();
    if (now - last_update_ms < UPDATE_INTERVAL_MS) {
        return;
    }
    
    // Only update if buffer has changed
    if (buffer_dirty) {
        updateDisplay();
        buffer_dirty = false;
        last_update_ms = now;
    }
}

void UITabTerminal::updateDisplay() {
    // Update display if terminal exists
    if (terminal_text) {
        lv_label_set_text(terminal_text, terminal_buffer.c_str());
        
        // Auto-scroll to bottom only if enabled
        if (terminal_cont && auto_scroll_enabled) {
            lv_obj_scroll_to_y(terminal_cont, LV_COORD_MAX, LV_ANIM_OFF);
        }
    }
}
