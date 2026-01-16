#ifndef UI_TAB_CONTROL_POWER_FEED_H
#define UI_TAB_CONTROL_POWER_FEED_H

#include <lvgl.h>

class UITabControlPowerFeed {
public:
    static void create(lv_obj_t *tab);
    static lv_obj_t *ta_x;
    static lv_obj_t *ta_y;
    static lv_obj_t *ta_z;

private:
    static lv_obj_t *parent_tab;
    static lv_obj_t *keyboard;
    static lv_obj_t *ta_feed;
    static lv_obj_t *mode_switch;
    static lv_obj_t *status_label;
    static lv_obj_t *encoder_switch;
    static lv_obj_t *active_field;
    static lv_timer_t *encoder_timer;
    static int16_t last_encoder_count;
    static bool encoder_enabled;

    static void showKeyboard(lv_obj_t *ta);
    static void hideKeyboard();
    static void onTextareaFocused(lv_event_t *e);
    static void onGoPressed(lv_event_t *e);
    static void onCurrentPressed(lv_event_t *e);
    static void onClearPressed(lv_event_t *e);
    static void onEncoderToggle(lv_event_t *e);
    static void encoderTimerCb(lv_timer_t *timer);
    static void setStatus(const char *text, lv_color_t color);
};

#endif // UI_TAB_CONTROL_POWER_FEED_H
