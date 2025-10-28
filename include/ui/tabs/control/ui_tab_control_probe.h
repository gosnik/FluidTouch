#ifndef UI_TAB_CONTROL_PROBE_H
#define UI_TAB_CONTROL_PROBE_H

#include <lvgl.h>

class UITabControlProbe {
public:
    static void create(lv_obj_t *tab);
    static void updateResult(const char* message);
    
    // Keyboard support
    static void showKeyboard(lv_obj_t *ta);
    static void hideKeyboard();

private:
    static lv_obj_t* results_text;
    static lv_obj_t* keyboard;
    static lv_obj_t* parent_tab;
    
    // Event handlers for probe buttons
    static void probe_x_minus_handler(lv_event_t* e);
    static void probe_x_plus_handler(lv_event_t* e);
    static void probe_y_minus_handler(lv_event_t* e);
    static void probe_y_plus_handler(lv_event_t* e);
    static void probe_z_minus_handler(lv_event_t* e);
    static void probe_z_plus_handler(lv_event_t* e);
    
    // Helper to execute probe command
    static void executeProbe(const char* axis, const char* direction);
};

#endif // UI_TAB_CONTROL_PROBE_H
