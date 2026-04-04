#ifndef UI_TAB_CONTROL_JOG_H
#define UI_TAB_CONTROL_JOG_H

#include <lvgl.h>
#include <cstddef>
#include <cstdint>
#include "core/qtdial_button_mapping.h"

struct FluidNCStatus;

class UITabControlJog {
public:
    static void create(lv_obj_t *tab);
    static void createSoftLimits(lv_obj_t *tab);
    static void registerNavigableNumericField(lv_obj_t *page, lv_obj_t *ta, char axis_hint);
    static void registerNavigableSwitch(lv_obj_t *page, lv_obj_t *obj);
    static void registerNavigableSlider(lv_obj_t *page, lv_obj_t *obj);
    static void registerNavigableButton(lv_obj_t *page, lv_obj_t *obj);
    static void registerNavigableButtons(lv_obj_t *page);
    static void setActiveNumericTextarea(lv_obj_t *ta, char axis_hint);
    static void clearActiveNumericTextarea(lv_obj_t *ta);
    static bool isNumericTextareaCaptureActive();
    static bool hasActiveNumericTextarea();
    static bool isFieldNavigationActive();
    static bool navigateActiveFieldSelection(int direction);
    static bool clickActiveNavigableSelection(bool long_press = false);
    static void beginFieldNavigationHold();
    static void endFieldNavigationHold();
    static float getCurrentXYFeed();
    static float getCurrentZFeed();
    static bool isRapidFeedEnabled();
    static void setRapidFeedEnabled(bool enabled);
    static void toggleRapidFeedEnabled();
    static void triggerMappedAction(QtdialButtonMappingTarget target);

private:
    enum SoftLimitMode : int8_t {
        SOFT_LIMIT_MODE_TRACK = -1,
        SOFT_LIMIT_MODE_OFF = 0,
        SOFT_LIMIT_MODE_ON = 1
    };

    static lv_obj_t *parent_tab;
    static lv_obj_t *xy_step_display_label;
    static lv_obj_t *z_step_display_label;
    static lv_obj_t *x_step_buttons[6];
    static lv_obj_t *y_step_buttons[6];
    static lv_obj_t *z_step_buttons[6];
    static lv_obj_t *xy_feedrate_label;
    static lv_obj_t *z_feedrate_label;
    static lv_obj_t *rapid_mode_label;
    static lv_obj_t *encoder_bind_container;
    static lv_obj_t *encoder_bind_buttons[3];
    static lv_timer_t *encoder_timer;
    static lv_obj_t *soft_limits_button;
    static lv_obj_t *soft_limits_overlay;
    static lv_obj_t *soft_limits_panel;
    static lv_obj_t *soft_limits_keyboard;
    static lv_obj_t *jog_keyboard;
    static lv_obj_t *soft_limits_active_ta;
    static lv_obj_t *active_numeric_ta;
    static lv_obj_t *active_navigable_obj;
    static char active_numeric_axis;
    static lv_obj_t *soft_limits_mode_slider_x;
    static lv_obj_t *soft_limits_mode_slider_y;
    static lv_obj_t *soft_limits_mode_slider_z;
    static lv_obj_t *soft_limits_mode_label_x;
    static lv_obj_t *soft_limits_mode_label_y;
    static lv_obj_t *soft_limits_mode_label_z;
    static lv_obj_t *soft_limits_x_min_ta;
    static lv_obj_t *soft_limits_x_max_ta;
    static lv_obj_t *soft_limits_y_min_ta;
    static lv_obj_t *soft_limits_y_max_ta;
    static lv_obj_t *soft_limits_z_min_ta;
    static lv_obj_t *soft_limits_z_max_ta;
    static lv_obj_t *jog_predicted_wpos_label;
    static lv_obj_t *jog_pending_cmd_count_label;
    enum class NavigableFieldType : uint8_t {
        Textarea,
        Switch,
        Slider,
        Button
    };
    struct NavigableFieldEntry {
        lv_obj_t *page;
        lv_obj_t *obj;
        NavigableFieldType type;
        char axis_hint;
    };
    static constexpr size_t kMaxNavigableFields = 256;
    static NavigableFieldEntry navigable_fields[kMaxNavigableFields];
    static size_t navigable_field_count;
    static bool field_navigation_moved;
    static int16_t last_encoder_counts[3];
    static int32_t last_override_count;
    static bool last_override_count_valid;
    static float x_current_step;
    static float y_current_step;
    static float z_current_step;
    static int x_current_step_index;
    static int y_current_step_index;
    static int z_current_step_index;
    static float xy_current_feed;
    static float z_current_feed;
    static bool rapid_feed_enabled;
    static bool soft_limit_x_enabled;
    static bool soft_limit_y_enabled;
    static bool soft_limit_z_enabled;
    static SoftLimitMode soft_limit_x_mode;
    static SoftLimitMode soft_limit_y_mode;
    static SoftLimitMode soft_limit_z_mode;
    static bool soft_limit_x_learn_initialized;
    static bool soft_limit_y_learn_initialized;
    static bool soft_limit_z_learn_initialized;
    static float soft_limit_x_min;
    static float soft_limit_x_max;
    static float soft_limit_y_min;
    static float soft_limit_y_max;
    static float soft_limit_z_min;
    static float soft_limit_z_max;
    
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
    static void soft_limits_button_event_cb(lv_event_t *e);
    static void soft_limits_close_event_cb(lv_event_t *e);
    static void soft_limits_save_event_cb(lv_event_t *e);
    static void soft_limit_mode_slider_event_cb(lv_event_t *e);
    static void soft_limits_textarea_focused_event_cb(lv_event_t *e);
    static void soft_limits_textarea_changed_event_cb(lv_event_t *e);
    static void jog_feed_textarea_focused_event_cb(lv_event_t *e);
    static void jog_feed_textarea_defocused_event_cb(lv_event_t *e);
    static void showJogKeyboard(lv_obj_t *ta);
    static void hideJogKeyboard();
    static void showSoftLimitsKeyboard(lv_obj_t *ta);
    static void hideSoftLimitsKeyboard();
    static void loadSoftLimitsFromConfig();
    static void saveSoftLimitsToConfig();
    static void applySoftLimitsToComm();
    static void updateSoftLimitModeLabel(char axis, SoftLimitMode mode);
    static SoftLimitMode sliderValueToSoftLimitMode(lv_obj_t *slider);
    static void setSliderFromSoftLimitMode(lv_obj_t *slider, SoftLimitMode mode);
    static void updateSoftLimitLearnTracking(const FluidNCStatus &status);
    static void syncSoftLimitsUI();
    static void storeSoftLimitsFromUI();
    static void updateJogDebugInfoUI();
    static float rapidXYFeedValue();
    static float rapidZFeedValue();
    static float effectiveXYFeedValue();
    static float effectiveZFeedValue();
    static void applyNavigableHighlight(lv_obj_t *obj, NavigableFieldType type, bool active);
    static void registerNavigableField(lv_obj_t *page, lv_obj_t *obj, NavigableFieldType type, char axis_hint);
    static void registerNavigableButtonsRecursive(lv_obj_t *page, lv_obj_t *root);
    static bool setActiveNavigableObject(lv_obj_t *obj, NavigableFieldType type, char axis_hint);
    static bool findNavigableEntry(lv_obj_t *obj, NavigableFieldEntry *entry_out = nullptr);
    
    // Jog button event handlers
    static void xy_jog_button_event_cb(lv_event_t *e);
    static void z_jog_button_event_cb(lv_event_t *e);
    static void cancel_jog_event_cb(lv_event_t *e);
};

#endif // UI_TAB_CONTROL_JOG_H
