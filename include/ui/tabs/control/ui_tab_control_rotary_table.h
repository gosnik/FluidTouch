#ifndef UI_TAB_CONTROL_ROTARY_TABLE_H
#define UI_TAB_CONTROL_ROTARY_TABLE_H

#include <lvgl.h>

class UITabControlRotaryTable {
public:
    static void create(lv_obj_t *tab);
    static lv_obj_t *ta_center_x;
    static lv_obj_t *ta_center_y;
    static lv_obj_t *ta_z;

private:
    static lv_obj_t *parent_tab;
    static lv_obj_t *keyboard;
    static lv_obj_t *ta_radius;
    static lv_obj_t *ta_arc;
    static lv_obj_t *ta_feed;
    static lv_obj_t *status_label;
    static lv_obj_t *encoder_switch;
    static lv_obj_t *active_field;
    static lv_timer_t *encoder_timer;
    static int16_t last_encoder_count;
    static bool encoder_enabled;

    static double start_x;
    static double start_y;
    static double current_x;
    static double current_y;

    static void showKeyboard(lv_obj_t *ta);
    static void hideKeyboard();
    static void onTextareaFocused(lv_event_t *e);
    static void onSetStartPressed(lv_event_t *e);
    static void onApplyRadiusPressed(lv_event_t *e);
    static void onApplyArcPressed(lv_event_t *e);
    static void onApplyZPressed(lv_event_t *e);
    static void onEncoderToggle(lv_event_t *e);
    static void encoderTimerCb(lv_timer_t *timer);
    static void setStatus(const char *text, lv_color_t color);

    static bool parseDouble(lv_obj_t *ta, double &out);
    static int parseFeed(lv_obj_t *ta);
    static void findArcSecondPoint(double &xp, double &yp, double xc, double yc, double length, bool clockwise);
    static void findNewPointByRadius(double &xp, double &yp, double xc, double yc, double new_radius);
};

#endif // UI_TAB_CONTROL_ROTARY_TABLE_H
