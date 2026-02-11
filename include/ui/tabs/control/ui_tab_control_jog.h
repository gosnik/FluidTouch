#ifndef UI_TAB_CONTROL_JOG_H
#define UI_TAB_CONTROL_JOG_H

#include <lvgl.h>

class UITabControlJog {
public:
    static void create(lv_obj_t *tab);

private:
    static lv_obj_t *parent_tab;
    static lv_obj_t *xy_step_display_label;
    static lv_obj_t *z_step_display_label;
    static lv_obj_t *x_step_buttons[6];
    static lv_obj_t *y_step_buttons[6];
    static lv_obj_t *z_step_buttons[6];
    static lv_obj_t *xy_feedrate_label;
    static lv_obj_t *z_feedrate_label;
    static lv_obj_t *encoder_bind_container;
    static lv_obj_t *encoder_bind_buttons[3];
    static lv_timer_t *encoder_timer;
    static int16_t last_encoder_counts[3];
    static int32_t last_override_count;
    static bool last_override_count_valid;
    static float x_current_step;
    static float y_current_step;
    static float z_current_step;
    static int x_current_step_index;
    static int y_current_step_index;
    static int z_current_step_index;
    static int xy_current_feed;
    static int z_current_feed;
    
    // Octagon stop button
    static void draw_octagon_event_cb(lv_event_t *e);
    static void x_step_button_event_cb(lv_event_t *e);
    static void y_step_button_event_cb(lv_event_t *e);
    static void z_step_button_event_cb(lv_event_t *e);
    static void xy_feedrate_adj_event_cb(lv_event_t *e);
    static void z_feedrate_adj_event_cb(lv_event_t *e);
    static void update_xy_step_display();
    static void update_z_step_display();
    static void update_x_step_button_styles();
    static void update_y_step_button_styles();
    static void update_z_step_button_styles();
    static void encoder_bind_button_event_cb(lv_event_t *e);
    static void update_encoder_bind_button_styles();
    static void reset_override_encoder_count();
    static void encoderTimerCb(lv_timer_t *timer);
    
    // Jog button event handlers
    static void xy_jog_button_event_cb(lv_event_t *e);
    static void z_jog_button_event_cb(lv_event_t *e);
    static void cancel_jog_event_cb(lv_event_t *e);
};

#endif // UI_TAB_CONTROL_JOG_H
