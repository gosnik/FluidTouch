#include "ui/tabs/ui_tab_terminal.h"
#include "ui/ui_theme.h"
#include "config.h"

// Static member initialization
lv_obj_t *UITabTerminal::terminal_text = nullptr;
lv_obj_t *UITabTerminal::input_field = nullptr;
lv_obj_t *UITabTerminal::keyboard = nullptr;

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
    lv_obj_set_size(input_field, SCREEN_WIDTH - (margin * 4) - button_width, input_height);
    lv_obj_set_pos(input_field, 0, 0);
    lv_textarea_set_one_line(input_field, true);
    lv_textarea_set_placeholder_text(input_field, "Enter command...");
    lv_obj_set_style_text_font(input_field, &lv_font_montserrat_18, 0);
    lv_obj_set_style_bg_color(input_field, UITheme::BG_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_text_color(input_field, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(input_field, input_field_event_cb, LV_EVENT_CLICKED, nullptr);
    
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
    lv_obj_t *terminal_cont = lv_obj_create(tab);
    lv_obj_set_size(terminal_cont, SCREEN_WIDTH - (margin * 4), terminal_height - (margin * 2));
    lv_obj_set_pos(terminal_cont, 3, input_height + (margin * 2));
    lv_obj_set_style_bg_color(terminal_cont, UITheme::BG_BLACK, LV_PART_MAIN);
    lv_obj_set_style_border_color(terminal_cont, UITheme::BORDER_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(terminal_cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(terminal_cont, 5, LV_PART_MAIN);
    // Enable scrolling for terminal output
    lv_obj_set_scroll_dir(terminal_cont, LV_DIR_VER);
    
    terminal_text = lv_label_create(terminal_cont);
    lv_label_set_text(terminal_text, 
        "[MSG:INFO: FluidNC v3.9.4 https://github.com/bdring/FluidNC.git]\n"
        "[MSG:INFO: Compiled with ESP32 SDK:v4.4.7-dirty]\n"
        "[MSG:INFO: Local filesystem type is spiffs]\n"
        "[MSG:INFO: Configuration file:config.yaml]\n"
        "[MSG:INFO: Machine WallPlotter]\n"
        "[MSG:INFO: Board Jackpot TMC2209]\n"
        "[MSG:INFO: UART1 Tx:gpio.0 Rx:gpio.4 RTS:NO_PIN Baud:115200]\n"
        "[MSG:INFO: I2SO BCK:gpio.22 WS:gpio.17 DATA:gpio.21Min Pulse:2us]\n"
        "[MSG:INFO: SPI SCK:gpio.18 MOSI:gpio.23 MISO:gpio.19]\n"
        "[MSG:INFO: SD Card cs_pin:gpio.5 detect:NO_PIN freq:20000000]\n"
        "[MSG:INFO: Stepping:I2S_STATIC Pulse:4us Dsbl Delay:0us Dir Delay:1us Idle Delay:255ms]\n"
        "[MSG:INFO: User Digital Output: 0 on Pin:gpio.26]\n"
        "[MSG:INFO: Axis count 3]\n"
        "[MSG:INFO: Axis X (0.000,640.000)]\n"
        "[MSG:INFO:   Motor0]\n"
        "[MSG:INFO:     tmc_2209 UART1 Addr:0 CS:NO_PIN Step:I2SO.2 Dir:I2SO.1 Disable:I2SO.0 R:0.110]\n"
        "[MSG:INFO:  X Neg Limit gpio.25]\n"
        "[MSG:INFO:   Motor1]\n"
        "[MSG:INFO:     tmc_2209 UART1 Addr:3 CS:I2SO.14 Step:I2SO.13 Dir:I2SO.12 Disable:I2SO.15 R:0.110]\n"
        "[MSG:INFO: Axis Y (0.000,625.000)]\n"
        "[MSG:INFO:   Motor0]\n"
        "[MSG:INFO:     tmc_2209 UART1 Addr:1 CS:NO_PIN Step:I2SO.5 Dir:I2SO.4 Disable:I2SO.7 R:0.110]\n"
        "[MSG:INFO:  Y Neg Limit gpio.33]\n"
        "[MSG:INFO: Axis Z (-16.000,0.000)]\n"
        "[MSG:INFO:   Motor0]\n"
        "[MSG:INFO:     tmc_2209 UART1 Addr:2 CS:NO_PIN Step:I2SO.10 Dir:I2SO.9 Disable:I2SO.8 R:0.110]\n"
        "[MSG:INFO:  Z Pos Limit gpio.32]\n"
        "[MSG:INFO: X Axis driver test passed]\n"
        "[MSG:INFO: X2 Axis driver test passed]\n"
        "[MSG:INFO: Y Axis driver test passed]\n"
        "[MSG:INFO: Z Axis driver test passed]\n"
        "[MSG:INFO: Kinematic system: Cartesian]\n"
        "[MSG:INFO: Connecting to STA SSID:TEST]\n"
        "[MSG:INFO: Connecting.]\n"
        "[MSG:INFO: Connecting..]\n"
        "[MSG:INFO: Connected - IP is 192.168.0.123]\n"
        "[MSG:INFO: WiFi on]\n"
        "[MSG:INFO: Start mDNS with hostname:http://fluidnc.local/]\n"
        "[MSG:INFO: HTTP started on port 80]\n"
        "[MSG:INFO: Telnet started on port 23]\n"
        "[MSG:INFO: BESC Spindle Out:gpio.27 Min:640us Max:1150us Freq:50Hz Full Period count:1048575 with m6_macro]\n"
        "[MSG:INFO: Probe gpio.36:low]\n"
        "ok\n"
        "<Idle|MPos:0.000,0.000,0.000|FS:0,0>");
    lv_obj_set_style_text_font(terminal_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(terminal_text, UITheme::UI_SUCCESS, 0);
    lv_label_set_long_mode(terminal_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(terminal_text, SCREEN_WIDTH - (margin * 4) - 10);
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

// Send command function
void UITabTerminal::send_command() {
    const char *cmd = lv_textarea_get_text(input_field);
    
    if (cmd != nullptr && strlen(cmd) > 0) {
        // Log to serial for debugging
        Serial.print("Terminal command: ");
        Serial.println(cmd);
        
        // TODO: Send command to FluidNC via serial/WiFi
        // For now, just echo it to the terminal display
        String current_text = lv_label_get_text(terminal_text);
        current_text += "\n> ";
        current_text += cmd;
        current_text += "\nok";  // Simulate response
        lv_label_set_text(terminal_text, current_text.c_str());
        
        // Clear input field
        lv_textarea_set_text(input_field, "");
        
        // Scroll to bottom of terminal
        lv_obj_scroll_to_y(lv_obj_get_parent(terminal_text), LV_COORD_MAX, LV_ANIM_ON);
    }
    
    // Hide keyboard after sending
    if (keyboard != nullptr) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}
