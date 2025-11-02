#ifndef UI_TAB_TERMINAL_H
#define UI_TAB_TERMINAL_H

#include <lvgl.h>
#include <Arduino.h>

class UITabTerminal {
public:
    static void create(lv_obj_t *tab);
    static void appendMessage(const char *message);  // Add WebSocket messages to terminal

private:
    static lv_obj_t *terminal_text;
    static lv_obj_t *terminal_cont;
    static lv_obj_t *input_field;
    static lv_obj_t *keyboard;
    static lv_obj_t *auto_scroll_switch;

    static String terminal_buffer;  // Buffer to store terminal text
    static const size_t MAX_BUFFER_SIZE = 8192;  // Limit buffer to 8KB
    static bool auto_scroll_enabled;  // Auto-scroll toggle state
    static bool buffer_dirty;  // Flag to indicate buffer needs UI update
    static uint32_t last_update_ms;  // Timestamp of last UI update
    static const uint32_t UPDATE_INTERVAL_MS = 100;  // Update UI every 100ms
    static bool in_json_message;  // Track if we're inside a multi-line JSON message
    static int json_brace_count;  // Track brace depth in JSON messages

    static void send_button_event_cb(lv_event_t *e);
    static void input_field_event_cb(lv_event_t *e);
    static void keyboard_event_cb(lv_event_t *e);
    static void auto_scroll_event_cb(lv_event_t *e);
    static void send_command();
    static void trimBuffer();  // Remove old lines if buffer is too large
    static void updateDisplay();  // Update the UI from buffer

public:
    static void update();  // Call periodically from main loop to update display
};

#endif // UI_TAB_TERMINAL_H
