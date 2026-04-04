#include "ui/tabs/control/ui_tab_control_jog.h"
#include "ui/tabs/control/ui_tab_control_actions.h"
#include "ui/tabs/ui_tab_macros.h"
#include "ui/tabs/ui_tab_control.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
#include "ui/ui_tabs.h"
#include "ui/machine_config.h"
#include "core/comm_manager.h"
#include "core/encoder.h"
#include "core/power_manager.h"
#include "core/usb_host_manager.h"
#include "config.h"
#include <Arduino.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Static member initialization
lv_obj_t *UITabControlJog::parent_tab = nullptr;
lv_obj_t *UITabControlJog::xy_step_display_label = nullptr;
lv_obj_t *UITabControlJog::z_step_display_label = nullptr;
lv_obj_t *UITabControlJog::x_step_buttons[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t *UITabControlJog::y_step_buttons[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t *UITabControlJog::z_step_buttons[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t *UITabControlJog::xy_feedrate_label = nullptr;
lv_obj_t *UITabControlJog::z_feedrate_label = nullptr;
lv_obj_t *UITabControlJog::rapid_mode_label = nullptr;
lv_obj_t *UITabControlJog::encoder_bind_container = nullptr;
lv_obj_t *UITabControlJog::encoder_bind_buttons[3] = {nullptr, nullptr, nullptr};
lv_timer_t *UITabControlJog::encoder_timer = nullptr;
lv_obj_t *UITabControlJog::soft_limits_button = nullptr;
lv_obj_t *UITabControlJog::soft_limits_panel = nullptr;
lv_obj_t *UITabControlJog::soft_limits_keyboard = nullptr;
lv_obj_t *UITabControlJog::jog_keyboard = nullptr;
lv_obj_t *UITabControlJog::soft_limits_active_ta = nullptr;
lv_obj_t *UITabControlJog::active_numeric_ta = nullptr;
lv_obj_t *UITabControlJog::active_navigable_obj = nullptr;
char UITabControlJog::active_numeric_axis = 'X';
lv_obj_t *UITabControlJog::soft_limits_mode_slider_x = nullptr;
lv_obj_t *UITabControlJog::soft_limits_mode_slider_y = nullptr;
lv_obj_t *UITabControlJog::soft_limits_mode_slider_z = nullptr;
lv_obj_t *UITabControlJog::soft_limits_mode_label_x = nullptr;
lv_obj_t *UITabControlJog::soft_limits_mode_label_y = nullptr;
lv_obj_t *UITabControlJog::soft_limits_mode_label_z = nullptr;
lv_obj_t *UITabControlJog::soft_limits_x_min_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_x_max_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_y_min_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_y_max_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_z_min_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_z_max_ta = nullptr;
lv_obj_t *UITabControlJog::jog_predicted_wpos_label = nullptr;
lv_obj_t *UITabControlJog::jog_pending_cmd_count_label = nullptr;
UITabControlJog::NavigableFieldEntry UITabControlJog::navigable_fields[UITabControlJog::kMaxNavigableFields] = {};
size_t UITabControlJog::navigable_field_count = 0;
bool UITabControlJog::field_navigation_moved = false;
int16_t UITabControlJog::last_encoder_counts[3] = {0, 0, 0};
int32_t UITabControlJog::last_override_count = 0;
bool UITabControlJog::last_override_count_valid = false;
float UITabControlJog::x_current_step = 0.01f;      // Will be loaded from settings
float UITabControlJog::y_current_step = 0.01f;      // Will be loaded from settings
float UITabControlJog::z_current_step = 0.01f;       // Will be loaded from settings
int UITabControlJog::x_current_step_index = 0;      // Will be recalculated
int UITabControlJog::y_current_step_index = 0;      // Will be recalculated
int UITabControlJog::z_current_step_index = 0;      // Will be recalculated
float UITabControlJog::xy_current_feed = 300.0f;        // Will be loaded from settings
float UITabControlJog::z_current_feed = 100.0f;         // Will be loaded from settings
bool UITabControlJog::rapid_feed_enabled = false;
bool UITabControlJog::soft_limit_x_enabled = false;
bool UITabControlJog::soft_limit_y_enabled = false;
bool UITabControlJog::soft_limit_z_enabled = false;
UITabControlJog::SoftLimitMode UITabControlJog::soft_limit_x_mode = static_cast<UITabControlJog::SoftLimitMode>(0);
UITabControlJog::SoftLimitMode UITabControlJog::soft_limit_y_mode = static_cast<UITabControlJog::SoftLimitMode>(0);
UITabControlJog::SoftLimitMode UITabControlJog::soft_limit_z_mode = static_cast<UITabControlJog::SoftLimitMode>(0);
bool UITabControlJog::soft_limit_x_learn_initialized = false;
bool UITabControlJog::soft_limit_y_learn_initialized = false;
bool UITabControlJog::soft_limit_z_learn_initialized = false;
float UITabControlJog::soft_limit_x_min = 0.0f;
float UITabControlJog::soft_limit_x_max = 0.0f;
float UITabControlJog::soft_limit_y_min = 0.0f;
float UITabControlJog::soft_limit_y_max = 0.0f;
float UITabControlJog::soft_limit_z_min = 0.0f;
float UITabControlJog::soft_limit_z_max = 0.0f;

namespace {
int axis_index_from_hint(char axis_hint) {
    if (axis_hint == 'Y' || axis_hint == 'y') {
        return 1;
    }
    if (axis_hint == 'Z' || axis_hint == 'z') {
        return 2;
    }
    return 0;
}

void trim_float_string(char *text) {
    if (!text) {
        return;
    }
    char *dot = strchr(text, '.');
    if (!dot) {
        return;
    }
    char *end = text + strlen(text) - 1;
    while (end > dot && *end == '0') {
        *end-- = '\0';
    }
    if (end == dot) {
        *end = '\0';
    }
}

bool parse_textarea_float(lv_obj_t *ta, float &out) {
    if (!ta) {
        return false;
    }
    const char *text = lv_textarea_get_text(ta);
    if (!text || text[0] == '\0') {
        return false;
    }
    char *end = nullptr;
    out = strtof(text, &end);
    return end && *end == '\0';
}

void set_textarea_float(lv_obj_t *ta, float value) {
    if (!ta) {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", value);
    lv_textarea_set_text(ta, buf);
}

void normalize_min_max_pair(lv_obj_t *min_ta, lv_obj_t *max_ta, lv_obj_t *changed_ta) {
    float min_v = 0.0f;
    float max_v = 0.0f;
    const bool has_min = parse_textarea_float(min_ta, min_v);
    const bool has_max = parse_textarea_float(max_ta, max_v);
    if (!has_min || !has_max) {
        return;
    }
    if (min_v <= max_v) {
        return;
    }
    if (changed_ta == min_ta) {
        set_textarea_float(max_ta, min_v);
    } else if (changed_ta == max_ta) {
        set_textarea_float(min_ta, max_v);
    } else {
        set_textarea_float(max_ta, min_v);
    }
}

float clamp_jog_feed(float value, float max_value) {
    if (value < 0.1f) {
        return 0.1f;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

float get_jog_feed_value(lv_obj_t *ta, float fallback, float max_value) {
    float value = fallback;
    if (ta) {
        float parsed = fallback;
        if (parse_textarea_float(ta, parsed)) {
            value = parsed;
        }
    }
    return clamp_jog_feed(value, max_value);
}

void set_jog_feed_text(lv_obj_t *ta, float value) {
    if (!ta) {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", value);
    trim_float_string(buf);
    lv_textarea_set_text(ta, buf);
}
} // namespace

void UITabControlJog::create(lv_obj_t *tab) {
    parent_tab = tab;
    // Load default values from settings
    UITabSettingsJog::loadPreferences();
    x_current_step = UITabSettingsJog::getDefaultXYStep();
    y_current_step = UITabSettingsJog::getDefaultXYStep();
    z_current_step = UITabSettingsJog::getDefaultZStep();
    xy_current_feed = UITabSettingsJog::getDefaultXYFeed();
    z_current_feed = UITabSettingsJog::getDefaultZFeed();
    loadSoftLimitsFromConfig();
    
    // Find closest X step index
    x_current_step_index = 2;  // Default to 10mm
    for (int i = 0; i < UITheme::XY_STEP_COUNT; i++) {
        if (fabs(UITheme::XY_STEP_VALUES[i] - x_current_step) < 0.01f) {
            x_current_step_index = i;
            break;
        }
    }
    // Find closest Y step index
    y_current_step_index = x_current_step_index;
    
    // Find closest Z step index
    z_current_step_index = 1;  // Default to 1mm
    for (int i = 0; i < UITheme::Z_STEP_COUNT; i++) {
        if (fabs(UITheme::Z_STEP_VALUES[i] - z_current_step) < 0.01f) {
            z_current_step_index = i;
            break;
        }
    }
    
    // Calculate available height - Control tab content area is ~370px
    
    // ========== XY Section (Left side) ==========
    
    // X Step size selection
    lv_obj_t *x_step_label = lv_label_create(tab);
    lv_label_set_text(x_step_label, "X Step");
    lv_obj_set_style_text_font(x_step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(x_step_label, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(x_step_label, UI_SCALE_X(5), UI_SCALE_Y(9));
    
    const int step_btn_width = UI_SCALE_X(40);
    const int step_btn_height = UI_SCALE_Y(40);
    const int step_btn_gap = UI_SCALE_X(4);
    const int xy_step_row_start_x = UI_SCALE_X(5);
    const int x_step_row_y = UI_SCALE_Y(30);
    const int y_step_row_y = UI_SCALE_Y(94);

    // X Step buttons - horizontal row, smallest to largest (left to right)
    for (int display_index = 0; display_index < UITheme::XY_STEP_COUNT; display_index++) {
        int value_index = UITheme::XY_STEP_COUNT - 1 - display_index;
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, step_btn_width, step_btn_height);
        lv_obj_set_pos(btn_step,
                       xy_step_row_start_x + display_index * (step_btn_width + step_btn_gap),
                       x_step_row_y);
        lv_obj_add_event_cb(btn_step, x_step_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)value_index);

        x_step_buttons[value_index] = btn_step;

        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, UITheme::XY_STEP_LABELS[value_index]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    update_x_step_button_styles();

    // Y Step size selection
    lv_obj_t *y_step_label = lv_label_create(tab);
    lv_label_set_text(y_step_label, "Y Step");
    lv_obj_set_style_text_font(y_step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(y_step_label, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(y_step_label, UI_SCALE_X(5), UI_SCALE_Y(74));

    // Y Step buttons - horizontal row, smallest to largest (left to right)
    for (int display_index = 0; display_index < UITheme::XY_STEP_COUNT; display_index++) {
        int value_index = UITheme::XY_STEP_COUNT - 1 - display_index;
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, step_btn_width, step_btn_height);
        lv_obj_set_pos(btn_step,
                       xy_step_row_start_x + display_index * (step_btn_width + step_btn_gap),
                       y_step_row_y);
        lv_obj_add_event_cb(btn_step, y_step_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)value_index);

        y_step_buttons[value_index] = btn_step;

        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, UITheme::XY_STEP_LABELS[value_index]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    update_y_step_button_styles();

    // Z Step size selection
    lv_obj_t *z_step_label = lv_label_create(tab);
    lv_label_set_text(z_step_label, "Z Step");
    lv_obj_set_style_text_font(z_step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(z_step_label, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(z_step_label, UI_SCALE_X(5), UI_SCALE_Y(139));

    const int z_step_row_start_x = UI_SCALE_X(5);
    const int z_step_row_y = UI_SCALE_Y(159);

    // Z Step size buttons - horizontal row, smallest to largest (left to right)
    for (int display_index = 0; display_index < UITheme::Z_STEP_COUNT; display_index++) {
        int value_index = UITheme::Z_STEP_COUNT - 1 - display_index;
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, step_btn_width, step_btn_height);
        lv_obj_set_pos(btn_step,
                       z_step_row_start_x + display_index * (step_btn_width + step_btn_gap),
                       z_step_row_y);
        lv_obj_add_event_cb(btn_step, z_step_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)value_index);

        z_step_buttons[value_index] = btn_step;

        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, UITheme::Z_STEP_LABELS[value_index]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    update_z_step_button_styles();

    // Encoder binding override (single qtdial only)
    encoder_bind_container = lv_obj_create(tab);
    lv_obj_set_size(encoder_bind_container, UI_SCALE_X(80), UI_SCALE_Y(210));
    lv_obj_set_pos(encoder_bind_container, UI_SCALE_X(270), UI_SCALE_Y(9));
    lv_obj_set_style_bg_opa(encoder_bind_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(encoder_bind_container, 0, 0);
    lv_obj_set_style_pad_all(encoder_bind_container, 0, 0);
    lv_obj_clear_flag(encoder_bind_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(encoder_bind_container, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *encoder_bind_label = lv_label_create(encoder_bind_container);
    lv_label_set_text(encoder_bind_label, "Encoder Bind");
    lv_obj_set_style_text_font(encoder_bind_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(encoder_bind_label, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(encoder_bind_label, 0, 0);

    static const char *kBindLabels[3] = {"X", "Y", "Z"};
    const int bind_btn_w = UI_SCALE_X(40);
    const int bind_btn_h = UI_SCALE_Y(40);
    const int bind_btn_gap = UI_SCALE_Y(25);
    const int bind_row_y = UI_SCALE_Y(20);
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *btn = lv_button_create(encoder_bind_container);
        lv_obj_set_size(btn, bind_btn_w, bind_btn_h);
        lv_obj_set_pos(btn, 80/2-bind_btn_w/2, bind_row_y + i * (bind_btn_h + bind_btn_gap));
        lv_obj_add_event_cb(btn, encoder_bind_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        encoder_bind_buttons[i] = btn;

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, kBindLabels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    update_encoder_bind_button_styles();
    if (encoder_bind_container) {
        if (UICommon::isEncoderBindVisible()) {
            lv_obj_clear_flag(encoder_bind_container, LV_OBJ_FLAG_HIDDEN);
            reset_override_encoder_count();
        } else {
            lv_obj_add_flag(encoder_bind_container, LV_OBJ_FLAG_HIDDEN);
        }
    }

    rapid_mode_label = lv_label_create(tab);
    lv_label_set_text(rapid_mode_label, "JOG: NORMAL");
    lv_obj_set_style_text_font(rapid_mode_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(rapid_mode_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(rapid_mode_label, UI_SCALE_X(520), UI_SCALE_Y(9));
    
    // XY Feed rate control
    lv_obj_t *xy_feed_label = lv_label_create(tab);
    lv_label_set_text(xy_feed_label, "XY Feed:");
    lv_obj_set_style_text_font(xy_feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(xy_feed_label, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(xy_feed_label, UI_SCALE_X(5), UI_SCALE_Y(210));
    
    xy_feedrate_label = lv_textarea_create(tab);
    lv_obj_set_size(xy_feedrate_label, UI_SCALE_X(90), UI_SCALE_Y(45));
    lv_obj_set_pos(xy_feedrate_label, UI_SCALE_X(250), UI_SCALE_Y(230));
    lv_textarea_set_one_line(xy_feedrate_label, true);
    lv_textarea_set_accepted_chars(xy_feedrate_label, "0123456789.");
    lv_obj_set_style_text_font(xy_feedrate_label, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(xy_feedrate_label, jog_feed_textarea_focused_event_cb, LV_EVENT_FOCUSED, reinterpret_cast<void *>(static_cast<intptr_t>('X')));
    lv_obj_add_event_cb(xy_feedrate_label, jog_feed_textarea_defocused_event_cb, LV_EVENT_DEFOCUSED, nullptr);
    char xy_feed_buf[16];
    snprintf(xy_feed_buf, sizeof(xy_feed_buf), "%.3f", xy_current_feed);
    trim_float_string(xy_feed_buf);
    lv_textarea_set_text(xy_feedrate_label, xy_feed_buf);
    registerNavigableNumericField(tab, xy_feedrate_label, 'X');
    
    // Now update XY step display (after feedrate label exists)
    update_xy_step_display();
    
    lv_obj_t *xy_feed_unit = lv_label_create(tab);
    lv_label_set_text(xy_feed_unit, "mm/min");
    lv_obj_set_style_text_font(xy_feed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(xy_feed_unit, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(xy_feed_unit, UI_SCALE_X(345), UI_SCALE_Y(242));
    
    // XY Feedrate adjustment buttons - all on one line: -100, -10, +10, +100
    lv_obj_t *btn_xy_minus100 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_minus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_minus100, UI_SCALE_X(5), UI_SCALE_Y(230));
    lv_obj_add_event_cb(btn_xy_minus100, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-100);
    lv_obj_t *lbl_xy_minus100 = lv_label_create(btn_xy_minus100);
    lv_label_set_text(lbl_xy_minus100, "-100");
    lv_obj_set_style_text_font(lbl_xy_minus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_minus100);
    
    lv_obj_t *btn_xy_minus10 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_minus10, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_minus10, UI_SCALE_X(5+(1*60)), UI_SCALE_Y(230));
    lv_obj_add_event_cb(btn_xy_minus10, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-10);
    lv_obj_t *lbl_xy_minus10 = lv_label_create(btn_xy_minus10);
    lv_label_set_text(lbl_xy_minus10, "-10");
    lv_obj_set_style_text_font(lbl_xy_minus10, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_minus10);
    
    lv_obj_t *btn_xy_plus10 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_plus10, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_plus10, UI_SCALE_X(5+(2*60)), UI_SCALE_Y(230));
    lv_obj_add_event_cb(btn_xy_plus10, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)10);
    lv_obj_t *lbl_xy_plus10 = lv_label_create(btn_xy_plus10);
    lv_label_set_text(lbl_xy_plus10, "+10");
    lv_obj_set_style_text_font(lbl_xy_plus10, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_plus10);
    
    lv_obj_t *btn_xy_plus100 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_plus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_plus100, UI_SCALE_X(5+(3*60)), UI_SCALE_Y(230));
    lv_obj_add_event_cb(btn_xy_plus100, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)100);
    lv_obj_t *lbl_xy_plus100 = lv_label_create(btn_xy_plus100);
    lv_label_set_text(lbl_xy_plus100, "+100");
    lv_obj_set_style_text_font(lbl_xy_plus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_plus100);
    
    z_step_display_label = nullptr;
    
    // Z Feed rate control
    lv_obj_t *z_feed_label = lv_label_create(tab);
    lv_label_set_text(z_feed_label, "Z Feed:");
    lv_obj_set_style_text_font(z_feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(z_feed_label, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(z_feed_label, UI_SCALE_X(5), UI_SCALE_Y(280));
    
    z_feedrate_label = lv_textarea_create(tab);
    lv_obj_set_size(z_feedrate_label, UI_SCALE_X(90), UI_SCALE_Y(45));
    lv_obj_set_pos(z_feedrate_label, UI_SCALE_X(250), UI_SCALE_Y(300));
    lv_textarea_set_one_line(z_feedrate_label, true);
    lv_textarea_set_accepted_chars(z_feedrate_label, "0123456789.");
    lv_obj_set_style_text_font(z_feedrate_label, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(z_feedrate_label, jog_feed_textarea_focused_event_cb, LV_EVENT_FOCUSED, reinterpret_cast<void *>(static_cast<intptr_t>('Z')));
    lv_obj_add_event_cb(z_feedrate_label, jog_feed_textarea_defocused_event_cb, LV_EVENT_DEFOCUSED, nullptr);
    char z_feed_buf[16];
    snprintf(z_feed_buf, sizeof(z_feed_buf), "%.3f", z_current_feed);
    trim_float_string(z_feed_buf);
    lv_textarea_set_text(z_feedrate_label, z_feed_buf);
    registerNavigableNumericField(tab, z_feedrate_label, 'Z');
    
    // Now update Z step display (after feedrate label exists)
    update_z_step_display();
    
    lv_obj_t *z_feed_unit = lv_label_create(tab);
    lv_label_set_text(z_feed_unit, "mm/min");
    lv_obj_set_style_text_font(z_feed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(z_feed_unit, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(z_feed_unit, UI_SCALE_X(345), UI_SCALE_Y(312));
    
    // Z Feedrate adjustment buttons - all on one line: -100, -10, +10, +100
    lv_obj_t *btn_z_minus100 = lv_button_create(tab);
    lv_obj_set_size(btn_z_minus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_minus100, UI_SCALE_X(5+(0*60)), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_minus100, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-100);
    lv_obj_t *lbl_z_minus100 = lv_label_create(btn_z_minus100);
    lv_label_set_text(lbl_z_minus100, "-100");
    lv_obj_set_style_text_font(lbl_z_minus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_minus100);
    
    lv_obj_t *btn_z_minus10 = lv_button_create(tab);
    lv_obj_set_size(btn_z_minus10, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_minus10, UI_SCALE_X(5+(1*60)), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_minus10, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-10);
    lv_obj_t *lbl_z_minus10 = lv_label_create(btn_z_minus10);
    lv_label_set_text(lbl_z_minus10, "-10");
    lv_obj_set_style_text_font(lbl_z_minus10, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_minus10);
    
    lv_obj_t *btn_z_plus10 = lv_button_create(tab);
    lv_obj_set_size(btn_z_plus10, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_plus10, UI_SCALE_X(5+(2*60)), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_plus10, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)10);
    lv_obj_t *lbl_z_plus10 = lv_label_create(btn_z_plus10);
    lv_label_set_text(lbl_z_plus10, "+10");
    lv_obj_set_style_text_font(lbl_z_plus10, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_plus10);
    
    lv_obj_t *btn_z_plus100 = lv_button_create(tab);
    lv_obj_set_size(btn_z_plus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_plus100, UI_SCALE_X(5+(3*60)), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_plus100, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)100);
    lv_obj_t *lbl_z_plus100 = lv_label_create(btn_z_plus100);
    lv_label_set_text(lbl_z_plus100, "+100");
    lv_obj_set_style_text_font(lbl_z_plus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_plus100);

    // ========== Cancel Jog Button (Bottom Right) ==========
    // Create a container for the octagon stop button
    lv_obj_t *btn_cancel = lv_obj_create(tab);
    lv_obj_set_size(btn_cancel, UI_SCALE_X(70), UI_SCALE_Y(70));
    lv_obj_set_pos(btn_cancel, UI_SCALE_X(520), UI_SCALE_Y(275));
    lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_pad_all(btn_cancel, 0, 0);
    
    // Add draw event to render octagon shape
    lv_obj_add_event_cb(btn_cancel, draw_octagon_event_cb, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(btn_cancel, cancel_jog_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "STOP");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_white(), 0);
    lv_obj_center(lbl_cancel);

    if (!encoder_timer) {
        encoder_timer = lv_timer_create(encoderTimerCb, 50, nullptr);
    }

    for (size_t i = 0; i < 3; ++i) {
        last_encoder_counts[i] = get_encoder_value(i);
    }
    reset_override_encoder_count();
}

void UITabControlJog::createSoftLimits(lv_obj_t *tab) {
    soft_limits_panel = lv_obj_create(tab);
    lv_obj_set_size(soft_limits_panel, UI_SCALE_X(360), UI_SCALE_Y(360));
    lv_obj_set_pos(soft_limits_panel, 0, 0);
    lv_obj_set_style_bg_color(soft_limits_panel, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_pad_all(soft_limits_panel, UI_SCALE_X(5), 0);
    lv_obj_set_style_border_width(soft_limits_panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(soft_limits_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(soft_limits_panel);
    lv_label_set_text(title, "SOFT LIMITS (WPos)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(title, UI_SCALE_X(5), UI_SCALE_Y(5));

    const lv_coord_t row_start_y = UI_SCALE_Y(25);
    const lv_coord_t row_gap = UI_SCALE_Y(65);
    const lv_coord_t axis_x = UI_SCALE_X(5);
    const lv_coord_t mode_slider_x = UI_SCALE_X(40);
    const lv_coord_t min_field_x = UI_SCALE_X(150);
    const lv_coord_t max_field_x = UI_SCALE_X(240);
    const lv_coord_t field_w = UI_SCALE_X(70);
    const lv_coord_t field_h = UI_SCALE_Y(40);

    lv_obj_t *lbl_min = lv_label_create(soft_limits_panel);
    lv_label_set_text(lbl_min, "Min:");
    lv_obj_set_style_text_font(lbl_min, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_min, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(lbl_min, min_field_x, UI_SCALE_Y(5));

    lv_obj_t *lbl_max = lv_label_create(soft_limits_panel);
    lv_label_set_text(lbl_max, "Max:");
    lv_obj_set_style_text_font(lbl_max, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_max, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(lbl_max, max_field_x, UI_SCALE_Y(5));

    auto make_axis_row = [&](const char *axis,
                             lv_coord_t y,
                             lv_obj_t **mode_slider_out,
                             lv_obj_t **mode_label_out,
                             lv_obj_t **min_out,
                             lv_obj_t **max_out) {
        lv_obj_t *lbl_axis = lv_label_create(soft_limits_panel);
        lv_label_set_text(lbl_axis, axis);
        lv_obj_set_style_text_font(lbl_axis, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_axis, UITheme::TEXT_MEDIUM, 0);
        lv_obj_set_pos(lbl_axis, axis_x, y + UI_SCALE_Y(5));

        lv_obj_t *mode_slider = lv_slider_create(soft_limits_panel);
        const lv_coord_t mode_slider_w = UI_SCALE_X(90);
        const lv_coord_t mode_slider_h = UI_SCALE_Y(30);
        lv_obj_set_size(mode_slider, mode_slider_w, mode_slider_h);
        lv_obj_set_pos(mode_slider, mode_slider_x, y + UI_SCALE_Y(6));
        lv_slider_set_range(mode_slider, -1, 1);
        lv_slider_set_value(mode_slider, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(mode_slider, UITheme::JOYSTICK_LINE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(mode_slider, UITheme::JOYSTICK_LINE, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(mode_slider, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(mode_slider, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(mode_slider, UI_SCALE_Y(6), LV_PART_MAIN);
        lv_obj_set_style_radius(mode_slider, UI_SCALE_Y(6), LV_PART_INDICATOR);
        lv_obj_set_style_border_width(mode_slider, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(mode_slider, 0, LV_PART_INDICATOR);
        lv_obj_set_style_outline_width(mode_slider, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(mode_slider, 0, LV_PART_INDICATOR);
        const lv_coord_t knob_w_target = mode_slider_w / 3;
        const lv_coord_t knob_h_target = mode_slider_h - UI_SCALE_Y(8);
        const lv_coord_t knob_pad_hor = (knob_w_target - mode_slider_h) / 2;
        const lv_coord_t knob_pad_ver = (knob_h_target - mode_slider_h) / 2;
        lv_obj_set_style_pad_hor(mode_slider, knob_w_target / 2, LV_PART_MAIN);
        lv_obj_set_style_pad_left(mode_slider, knob_pad_hor, LV_PART_KNOB);
        lv_obj_set_style_pad_right(mode_slider, knob_pad_hor, LV_PART_KNOB);
        lv_obj_set_style_pad_top(mode_slider, knob_pad_ver, LV_PART_KNOB);
        lv_obj_set_style_pad_bottom(mode_slider, knob_pad_ver, LV_PART_KNOB);
        lv_obj_set_style_radius(mode_slider, UI_SCALE_Y(5), LV_PART_KNOB);
        lv_obj_set_ext_click_area(mode_slider, LV_DPX(8));
        lv_obj_add_event_cb(mode_slider, soft_limit_mode_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)axis[0]);
        lv_obj_add_event_cb(mode_slider, soft_limit_mode_slider_event_cb, LV_EVENT_RELEASED, (void *)(intptr_t)axis[0]);
        *mode_slider_out = mode_slider;

        lv_obj_t *mode_lbl = lv_label_create(soft_limits_panel);
        lv_label_set_text(mode_lbl, "Off");
        lv_obj_set_style_text_font(mode_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(mode_lbl, UITheme::TEXT_MEDIUM, 0);
        lv_obj_set_pos(mode_lbl, axis_x, y + UI_SCALE_Y(20));
        *mode_label_out = mode_lbl;

        lv_obj_t *ta_min = lv_textarea_create(soft_limits_panel);
        lv_obj_set_size(ta_min, field_w, field_h);
        lv_obj_set_pos(ta_min, min_field_x, y);
        lv_textarea_set_one_line(ta_min, true);
        lv_textarea_set_max_length(ta_min, 10);
        lv_textarea_set_accepted_chars(ta_min, "0123456789.-");
        lv_obj_set_style_text_font(ta_min, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta_min, soft_limits_textarea_focused_event_cb, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(ta_min, soft_limits_textarea_changed_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
        *min_out = ta_min;

        lv_obj_t *ta_max = lv_textarea_create(soft_limits_panel);
        lv_obj_set_size(ta_max, field_w, field_h);
        lv_obj_set_pos(ta_max, max_field_x, y);
        lv_textarea_set_one_line(ta_max, true);
        lv_textarea_set_max_length(ta_max, 10);
        lv_textarea_set_accepted_chars(ta_max, "0123456789.-");
        lv_obj_set_style_text_font(ta_max, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta_max, soft_limits_textarea_focused_event_cb, LV_EVENT_FOCUSED, nullptr);
        lv_obj_add_event_cb(ta_max, soft_limits_textarea_changed_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
        *max_out = ta_max;
    };

    make_axis_row("X", row_start_y, &soft_limits_mode_slider_x, &soft_limits_mode_label_x, &soft_limits_x_min_ta, &soft_limits_x_max_ta);
    make_axis_row("Y", row_start_y + row_gap, &soft_limits_mode_slider_y, &soft_limits_mode_label_y, &soft_limits_y_min_ta, &soft_limits_y_max_ta);
    make_axis_row("Z", row_start_y + 2 * row_gap, &soft_limits_mode_slider_z, &soft_limits_mode_label_z, &soft_limits_z_min_ta, &soft_limits_z_max_ta);
    registerNavigableSlider(tab, soft_limits_mode_slider_x);
    registerNavigableSlider(tab, soft_limits_mode_slider_y);
    registerNavigableSlider(tab, soft_limits_mode_slider_z);
    registerNavigableNumericField(tab, soft_limits_x_min_ta, 'X');
    registerNavigableNumericField(tab, soft_limits_x_max_ta, 'X');
    registerNavigableNumericField(tab, soft_limits_y_min_ta, 'Y');
    registerNavigableNumericField(tab, soft_limits_y_max_ta, 'Y');
    registerNavigableNumericField(tab, soft_limits_z_min_ta, 'Z');
    registerNavigableNumericField(tab, soft_limits_z_max_ta, 'Z');

    lv_obj_t *btn_save = lv_button_create(soft_limits_panel);
    lv_obj_set_size(btn_save, UI_SCALE_X(150), UI_SCALE_Y(44));
    lv_obj_set_pos(btn_save, UI_SCALE_X(160), UI_SCALE_Y(220));
    lv_obj_set_style_bg_color(btn_save, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(btn_save, soft_limits_save_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_save);

    jog_predicted_wpos_label = lv_label_create(soft_limits_panel);
    lv_label_set_text(jog_predicted_wpos_label, "Pred WPos\nX: 0.000\nY: 0.000\nZ: 0.000");
    lv_obj_set_style_text_font(jog_predicted_wpos_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jog_predicted_wpos_label, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(jog_predicted_wpos_label, UI_SCALE_X(5), UI_SCALE_Y(270));

    jog_pending_cmd_count_label = lv_label_create(soft_limits_panel);
    lv_label_set_text(jog_pending_cmd_count_label, "Pending Cmds: 0");
    lv_obj_set_style_text_font(jog_pending_cmd_count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(jog_pending_cmd_count_label, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(jog_pending_cmd_count_label, UI_SCALE_X(5), UI_SCALE_Y(338));

    syncSoftLimitsUI();
    updateJogDebugInfoUI();
}

void UITabControlJog::registerNavigableField(lv_obj_t *page, lv_obj_t *obj, NavigableFieldType type, char axis_hint) {
    if (!obj) {
        return;
    }

    for (size_t i = 0; i < navigable_field_count; ++i) {
        if (navigable_fields[i].obj != obj) {
            continue;
        }
        navigable_fields[i].page = page;
        navigable_fields[i].type = type;
        navigable_fields[i].axis_hint = axis_hint;
        return;
    }

    if (navigable_field_count >= kMaxNavigableFields) {
        return;
    }

    navigable_fields[navigable_field_count++] = {page, obj, type, axis_hint};
}

void UITabControlJog::registerNavigableNumericField(lv_obj_t *page, lv_obj_t *ta, char axis_hint) {
    registerNavigableField(page, ta, NavigableFieldType::Textarea, axis_hint);
}

void UITabControlJog::registerNavigableSwitch(lv_obj_t *page, lv_obj_t *obj) {
    registerNavigableField(page, obj, NavigableFieldType::Switch, 'X');
}

void UITabControlJog::registerNavigableSlider(lv_obj_t *page, lv_obj_t *obj) {
    registerNavigableField(page, obj, NavigableFieldType::Slider, 'X');
}

void UITabControlJog::registerNavigableButton(lv_obj_t *page, lv_obj_t *obj) {
    registerNavigableField(page, obj, NavigableFieldType::Button, 'X');
}

void UITabControlJog::registerNavigableButtonsRecursive(lv_obj_t *page, lv_obj_t *root) {
    if (!root || !lv_obj_is_valid(root)) {
        return;
    }

    if (lv_obj_has_class(root, &lv_button_class)) {
        registerNavigableField(page, root, NavigableFieldType::Button, 'X');
    }

    const uint32_t child_count = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < child_count; ++i) {
        registerNavigableButtonsRecursive(page, lv_obj_get_child(root, i));
    }
}

void UITabControlJog::registerNavigableButtons(lv_obj_t *page) {
    registerNavigableButtonsRecursive(page, page);
}

bool UITabControlJog::findNavigableEntry(lv_obj_t *obj, NavigableFieldEntry *entry_out) {
    if (!obj) {
        return false;
    }
    for (size_t i = 0; i < navigable_field_count; ++i) {
        if (navigable_fields[i].obj != obj) {
            continue;
        }
        if (entry_out) {
            *entry_out = navigable_fields[i];
        }
        return true;
    }
    return false;
}

void UITabControlJog::applyNavigableHighlight(lv_obj_t *obj, NavigableFieldType type, bool active) {
    if (!obj || !lv_obj_is_valid(obj)) {
        return;
    }

    const lv_color_t color = active ? UITheme::ACCENT_SECONDARY : UITheme::BORDER_LIGHT;
    const lv_coord_t width = active ? UI_SCALE_X(3) : 0;
    lv_obj_set_style_outline_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_outline_width(obj, width, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(obj, UI_SCALE_X(2), LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, active ? UITheme::ACCENT_SECONDARY : UITheme::BORDER_LIGHT, LV_PART_MAIN);
    if (type == NavigableFieldType::Textarea) {
        lv_obj_set_style_border_width(obj, active ? UI_SCALE_X(2) : UI_SCALE_X(1), LV_PART_MAIN);
    } else {
        lv_obj_set_style_border_width(obj, active ? UI_SCALE_X(2) : 0, LV_PART_MAIN);
    }
}

bool UITabControlJog::setActiveNavigableObject(lv_obj_t *obj, NavigableFieldType type, char axis_hint) {
    if (!obj) {
        return false;
    }
    if (active_navigable_obj && active_navigable_obj != obj) {
        NavigableFieldEntry previous{};
        if (findNavigableEntry(active_navigable_obj, &previous)) {
            applyNavigableHighlight(active_navigable_obj, previous.type, false);
        }
    }
    active_navigable_obj = obj;
    active_numeric_ta = (type == NavigableFieldType::Textarea) ? obj : nullptr;
    applyNavigableHighlight(active_navigable_obj, type, true);

    active_numeric_axis = axis_hint ? axis_hint : 'X';
    return true;
}

void UITabControlJog::setActiveNumericTextarea(lv_obj_t *ta, char axis_hint) {
    if (!ta) {
        return;
    }
    setActiveNavigableObject(ta, NavigableFieldType::Textarea, axis_hint);
}

void UITabControlJog::clearActiveNumericTextarea(lv_obj_t *ta) {
    if (!active_navigable_obj) {
        return;
    }

    if (!ta) {
        lv_obj_t *current = active_navigable_obj;
        NavigableFieldEntry current_entry{};
        const bool has_entry = findNavigableEntry(current, &current_entry);
        if (has_entry) {
            applyNavigableHighlight(current, current_entry.type, false);
            if (current_entry.type == NavigableFieldType::Textarea) {
                UICommon::clearKeyboardTarget(current);
            }
        }
        active_navigable_obj = nullptr;
        active_numeric_ta = nullptr;
        active_numeric_axis = 'X';
        for (size_t i = 0; i < navigable_field_count; ++i) {
            lv_obj_t *field = navigable_fields[i].obj;
            if (!field || !lv_obj_is_valid(field) || lv_obj_has_flag(field, LV_OBJ_FLAG_HIDDEN)) {
                continue;
            }
            if (navigable_fields[i].type == NavigableFieldType::Textarea) {
                lv_obj_send_event(field, LV_EVENT_DEFOCUSED, nullptr);
            }
        }
        return;
    }

    if (active_navigable_obj == ta) {
        NavigableFieldEntry current_entry{};
        if (findNavigableEntry(active_navigable_obj, &current_entry)) {
            applyNavigableHighlight(active_navigable_obj, current_entry.type, false);
            if (current_entry.type == NavigableFieldType::Textarea) {
                UICommon::clearKeyboardTarget(active_navigable_obj);
            }
        }
        active_navigable_obj = nullptr;
        active_numeric_ta = nullptr;
        active_numeric_axis = 'X';
    }
}

bool UITabControlJog::isNumericTextareaCaptureActive() {
    return active_navigable_obj != nullptr;
}

bool UITabControlJog::hasActiveNumericTextarea() {
    return active_navigable_obj != nullptr;
}

bool UITabControlJog::isFieldNavigationActive() {
    return QtdialButtonMappingManager::isTargetHeld(QtdialButtonMappingTarget::NavigateFieldsHold);
}

void UITabControlJog::beginFieldNavigationHold() {
    field_navigation_moved = false;
}

void UITabControlJog::endFieldNavigationHold() {
    if (!field_navigation_moved) {
        clearActiveNumericTextarea(nullptr);
    }
    field_navigation_moved = false;
}

bool UITabControlJog::navigateActiveFieldSelection(int direction) {
    if (direction == 0) {
        return false;
    }

    lv_obj_t *active_page = nullptr;
    switch (UITabs::getActiveTab()) {
        case 1:
            active_page = UITabControl::getActivePage();
            break;
        case 3:
            active_page = UITabMacros::getNavigationPage();
            break;
        default:
            break;
    }
    size_t active_page_indices[kMaxNavigableFields] = {};
    size_t active_page_count = 0;
    for (size_t i = 0; i < navigable_field_count; ++i) {
        const NavigableFieldEntry &entry = navigable_fields[i];
        if (!entry.obj || !lv_obj_is_valid(entry.obj)) {
            continue;
        }
        if (entry.page && (!lv_obj_is_valid(entry.page) || lv_obj_has_flag(entry.page, LV_OBJ_FLAG_HIDDEN))) {
            continue;
        }
        if (lv_obj_has_flag(entry.obj, LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        if (active_page && entry.page == active_page) {
            active_page_indices[active_page_count++] = i;
        }
    }

    if (active_page_count == 0) {
        return false;
    }

    for (size_t i = 0; i + 1 < active_page_count; ++i) {
        for (size_t j = i + 1; j < active_page_count; ++j) {
            const NavigableFieldEntry &lhs = navigable_fields[active_page_indices[i]];
            const NavigableFieldEntry &rhs = navigable_fields[active_page_indices[j]];
            lv_area_t lhs_coords{};
            lv_area_t rhs_coords{};
            lv_obj_get_coords(lhs.obj, &lhs_coords);
            lv_obj_get_coords(rhs.obj, &rhs_coords);

            const lv_coord_t lhs_y = lhs_coords.y1;
            const lv_coord_t rhs_y = rhs_coords.y1;
            const lv_coord_t lhs_x = lhs_coords.x1;
            const lv_coord_t rhs_x = rhs_coords.x1;
            constexpr lv_coord_t kSameRowThreshold = 12;

            const bool swap =
                (rhs_y < lhs_y - kSameRowThreshold) ||
                (std::abs(rhs_y - lhs_y) <= kSameRowThreshold && rhs_x < lhs_x);
            if (swap) {
                const size_t tmp = active_page_indices[i];
                active_page_indices[i] = active_page_indices[j];
                active_page_indices[j] = tmp;
            }
        }
    }

    size_t selected_pos = 0;
    bool found_current = false;
    for (size_t pos = 0; pos < active_page_count; ++pos) {
        if (navigable_fields[active_page_indices[pos]].obj == active_navigable_obj) {
            selected_pos = pos;
            found_current = true;
            break;
        }
    }

    if (!found_current) {
        const size_t selected_index = active_page_indices[(direction > 0) ? 0 : (active_page_count - 1)];
        const NavigableFieldEntry &entry = navigable_fields[selected_index];
        setActiveNavigableObject(entry.obj, entry.type, entry.axis_hint);
        field_navigation_moved = true;
        PowerManager::onUserActivity();
        return true;
    } else if (direction > 0) {
        selected_pos = (selected_pos + 1) % active_page_count;
    } else {
        selected_pos = (selected_pos == 0) ? (active_page_count - 1) : (selected_pos - 1);
    }

    const NavigableFieldEntry &entry = navigable_fields[active_page_indices[selected_pos]];
    setActiveNavigableObject(entry.obj, entry.type, entry.axis_hint);
    field_navigation_moved = true;
    PowerManager::onUserActivity();
    return true;
}

bool UITabControlJog::clickActiveNavigableSelection(bool long_press) {
    if (!active_navigable_obj || !lv_obj_is_valid(active_navigable_obj)) {
        return false;
    }

    NavigableFieldEntry entry{};
    if (!findNavigableEntry(active_navigable_obj, &entry)) {
        return false;
    }

    PowerManager::onUserActivity();

    switch (entry.type) {
        case NavigableFieldType::Textarea:
            lv_obj_send_event(active_navigable_obj, LV_EVENT_FOCUSED, nullptr);
            return true;
        case NavigableFieldType::Switch:
            if (lv_obj_has_state(active_navigable_obj, LV_STATE_CHECKED)) {
                lv_obj_remove_state(active_navigable_obj, LV_STATE_CHECKED);
            } else {
                lv_obj_add_state(active_navigable_obj, LV_STATE_CHECKED);
            }
            lv_obj_send_event(active_navigable_obj, LV_EVENT_VALUE_CHANGED, nullptr);
            return true;
        case NavigableFieldType::Slider:
            lv_obj_send_event(active_navigable_obj,
                              long_press ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED,
                              nullptr);
            return true;
        case NavigableFieldType::Button:
            lv_obj_send_event(active_navigable_obj,
                              long_press ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED,
                              nullptr);
            return true;
    }

    return false;
}

float UITabControlJog::getCurrentXYFeed() {
    return effectiveXYFeedValue();
}

float UITabControlJog::getCurrentZFeed() {
    return effectiveZFeedValue();
}

bool UITabControlJog::isRapidFeedEnabled() {
    return rapid_feed_enabled;
}

void UITabControlJog::setRapidFeedEnabled(bool enabled) {
    rapid_feed_enabled = enabled;
    if (rapid_mode_label) {
        lv_label_set_text(rapid_mode_label, enabled ? "JOG: RAPID" : "JOG: NORMAL");
        lv_obj_set_style_text_color(rapid_mode_label,
                                    enabled ? UITheme::UI_WARNING : UITheme::UI_INFO,
                                    0);
    }
    update_xy_step_display();
    update_z_step_display();
}

void UITabControlJog::toggleRapidFeedEnabled() {
    setRapidFeedEnabled(!rapid_feed_enabled);
}

float UITabControlJog::rapidXYFeedValue() {
    return clamp_jog_feed(static_cast<float>(UITabSettingsJog::getRapidXYFeed()), 15000.0f);
}

float UITabControlJog::rapidZFeedValue() {
    return clamp_jog_feed(static_cast<float>(UITabSettingsJog::getRapidZFeed()), 10000.0f);
}

float UITabControlJog::effectiveXYFeedValue() {
    return rapid_feed_enabled
               ? rapidXYFeedValue()
               : get_jog_feed_value(xy_feedrate_label, xy_current_feed, 10000.0f);
}

float UITabControlJog::effectiveZFeedValue() {
    return rapid_feed_enabled
               ? rapidZFeedValue()
               : get_jog_feed_value(z_feedrate_label, z_current_feed, 5000.0f);
}

void UITabControlJog::triggerMappedAction(QtdialButtonMappingTarget target) {
    auto active_axis_index = []() -> int {
        const int axis = UICommon::getEncoderBindAxis();
        if (axis < 0) {
            return 0;
        }
        if (axis > 2) {
            return 2;
        }
        return axis;
    };

    auto adjust_xy_feed = [](int adjustment) {
        if (!xy_feedrate_label) {
            return;
        }
        float current_value = get_jog_feed_value(xy_feedrate_label, xy_current_feed, 10000.0f);
        float new_value = clamp_jog_feed(current_value + static_cast<float>(adjustment), 10000.0f);
        set_jog_feed_text(xy_feedrate_label, new_value);
        xy_current_feed = new_value;
        update_xy_step_display();
    };

    auto adjust_z_feed = [](int adjustment) {
        if (!z_feedrate_label) {
            return;
        }
        float current_value = get_jog_feed_value(z_feedrate_label, z_current_feed, 5000.0f);
        float new_value = clamp_jog_feed(current_value + static_cast<float>(adjustment), 5000.0f);
        set_jog_feed_text(z_feedrate_label, new_value);
        z_current_feed = new_value;
        update_z_step_display();
    };

    auto adjust_active_feed = [&](int adjustment) {
        if (active_axis_index() == 2) {
            adjust_z_feed(adjustment);
        } else {
            adjust_xy_feed(adjustment);
        }
    };

    auto set_x_step = [](int index) {
        x_current_step_index = index;
        x_current_step = UITheme::XY_STEP_VALUES[index];
        update_xy_step_display();
        update_x_step_button_styles();
    };

    auto set_y_step = [](int index) {
        y_current_step_index = index;
        y_current_step = UITheme::XY_STEP_VALUES[index];
        update_xy_step_display();
        update_y_step_button_styles();
    };

    auto set_z_step = [](int index) {
        z_current_step_index = index;
        z_current_step = UITheme::Z_STEP_VALUES[index];
        update_z_step_display();
        update_z_step_button_styles();
    };

    auto set_active_step_value = [&](float value) {
        const int axis = active_axis_index();
        if (axis == 2) {
            int best = 0;
            float best_diff = fabsf(UITheme::Z_STEP_VALUES[0] - value);
            for (int i = 1; i < UITheme::Z_STEP_COUNT; ++i) {
                const float diff = fabsf(UITheme::Z_STEP_VALUES[i] - value);
                if (diff < best_diff) {
                    best = i;
                    best_diff = diff;
                }
            }
            set_z_step(best);
            return;
        }

        int best = 0;
        float best_diff = fabsf(UITheme::XY_STEP_VALUES[0] - value);
        for (int i = 1; i < UITheme::XY_STEP_COUNT; ++i) {
            const float diff = fabsf(UITheme::XY_STEP_VALUES[i] - value);
            if (diff < best_diff) {
                best = i;
                best_diff = diff;
            }
        }
        if (axis == 1) {
            set_y_step(best);
        } else {
            set_x_step(best);
        }
    };

    auto select_axis = [&](int axis) {
        UICommon::setEncoderBindAxis(axis, true);
        update_encoder_bind_button_styles();
        reset_override_encoder_count();
    };

    auto send_xy = [](float x_move, float y_move) {
        if (!CommManager::isConnected()) {
            return;
        }
        const float feedrate = effectiveXYFeedValue();
        CommManager::sendJogRelative(x_move, y_move, 0.0f, feedrate);
    };

    auto send_z = [](int direction) {
        if (!CommManager::isConnected()) {
            return;
        }
        const float feedrate = effectiveZFeedValue();
        CommManager::sendJogRelative(0.0f, 0.0f, z_current_step * direction, feedrate);
    };

    PowerManager::onUserActivity();

    switch (target) {
        case QtdialButtonMappingTarget::JogNorthWest: send_xy(-x_current_step, y_current_step); break;
        case QtdialButtonMappingTarget::JogNorth: send_xy(0.0f, y_current_step); break;
        case QtdialButtonMappingTarget::JogNorthEast: send_xy(x_current_step, y_current_step); break;
        case QtdialButtonMappingTarget::JogWest: send_xy(-x_current_step, 0.0f); break;
        case QtdialButtonMappingTarget::JogEast: send_xy(x_current_step, 0.0f); break;
        case QtdialButtonMappingTarget::JogSouthWest: send_xy(-x_current_step, -y_current_step); break;
        case QtdialButtonMappingTarget::JogSouth: send_xy(0.0f, -y_current_step); break;
        case QtdialButtonMappingTarget::JogSouthEast: send_xy(x_current_step, -y_current_step); break;
        case QtdialButtonMappingTarget::JogZPlus: send_z(1); break;
        case QtdialButtonMappingTarget::JogZMinus: send_z(-1); break;
        case QtdialButtonMappingTarget::Stop:
            if (CommManager::isConnected()) {
                CommManager::sendCommand("\x85");
            }
            break;
        case QtdialButtonMappingTarget::FeedMinus100: adjust_active_feed(-100); break;
        case QtdialButtonMappingTarget::FeedMinus10: adjust_active_feed(-10); break;
        case QtdialButtonMappingTarget::FeedPlus10: adjust_active_feed(10); break;
        case QtdialButtonMappingTarget::FeedPlus100: adjust_active_feed(100); break;
        case QtdialButtonMappingTarget::RapidFeedToggle: toggleRapidFeedEnabled(); break;
        case QtdialButtonMappingTarget::Step100: set_active_step_value(100.0f); break;
        case QtdialButtonMappingTarget::Step50: set_active_step_value(50.0f); break;
        case QtdialButtonMappingTarget::Step25: set_active_step_value(25.0f); break;
        case QtdialButtonMappingTarget::Step10: set_active_step_value(10.0f); break;
        case QtdialButtonMappingTarget::Step1: set_active_step_value(1.0f); break;
        case QtdialButtonMappingTarget::Step0_1: set_active_step_value(0.1f); break;
        case QtdialButtonMappingTarget::Step0_01: set_active_step_value(0.01f); break;
        case QtdialButtonMappingTarget::SelectAxisX: select_axis(0); break;
        case QtdialButtonMappingTarget::SelectAxisY: select_axis(1); break;
        case QtdialButtonMappingTarget::SelectAxisZ: select_axis(2); break;
        case QtdialButtonMappingTarget::NavigateFieldsHold: break;
        case QtdialButtonMappingTarget::CycleTopTabs:
            clearActiveNumericTextarea(nullptr);
            UITabs::cycleTabs();
            break;
        case QtdialButtonMappingTarget::CycleControlPages:
            clearActiveNumericTextarea(nullptr);
            UITabs::setActiveTab(1, LV_ANIM_OFF);
            UITabControl::cycleSubtabs();
            break;
        case QtdialButtonMappingTarget::ActionPauseResume:
            UITabControlActions::triggerAction(UITabControlActions::Action::PauseResume);
            break;
        case QtdialButtonMappingTarget::ActionUnlock:
            UITabControlActions::triggerAction(UITabControlActions::Action::Unlock);
            break;
        case QtdialButtonMappingTarget::ActionSoftReset:
            UITabControlActions::triggerAction(UITabControlActions::Action::SoftReset);
            break;
        case QtdialButtonMappingTarget::ActionQuickStop:
            UITabControlActions::triggerAction(UITabControlActions::Action::QuickStop);
            break;
        case QtdialButtonMappingTarget::ActionHomeX:
            UITabControlActions::triggerAction(UITabControlActions::Action::HomeX);
            break;
        case QtdialButtonMappingTarget::ActionHomeY:
            UITabControlActions::triggerAction(UITabControlActions::Action::HomeY);
            break;
        case QtdialButtonMappingTarget::ActionHomeZ:
            UITabControlActions::triggerAction(UITabControlActions::Action::HomeZ);
            break;
        case QtdialButtonMappingTarget::ActionHomeAll:
            UITabControlActions::triggerAction(UITabControlActions::Action::HomeAll);
            break;
        case QtdialButtonMappingTarget::ActionZeroX:
            UITabControlActions::triggerAction(UITabControlActions::Action::ZeroX);
            break;
        case QtdialButtonMappingTarget::ActionZeroY:
            UITabControlActions::triggerAction(UITabControlActions::Action::ZeroY);
            break;
        case QtdialButtonMappingTarget::ActionZeroZ:
            UITabControlActions::triggerAction(UITabControlActions::Action::ZeroZ);
            break;
        case QtdialButtonMappingTarget::ActionZeroAll:
            UITabControlActions::triggerAction(UITabControlActions::Action::ZeroAll);
            break;
        case QtdialButtonMappingTarget::MacroSlot1:
        case QtdialButtonMappingTarget::MacroSlot2:
        case QtdialButtonMappingTarget::MacroSlot3:
        case QtdialButtonMappingTarget::MacroSlot4:
        case QtdialButtonMappingTarget::MacroSlot5:
        case QtdialButtonMappingTarget::MacroSlot6:
        case QtdialButtonMappingTarget::MacroSlot7:
        case QtdialButtonMappingTarget::MacroSlot8:
        case QtdialButtonMappingTarget::MacroSlot9:
        case QtdialButtonMappingTarget::MacroRecordToggle:
        case QtdialButtonMappingTarget::ClickNavigateSelection:
            break;
        case QtdialButtonMappingTarget::Count: break;
    }
}

// X Step button event handler
void UITabControlJog::x_step_button_event_cb(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    x_current_step_index = index;
    x_current_step = UITheme::XY_STEP_VALUES[index];
    update_xy_step_display();
    update_x_step_button_styles();
    
    Serial.printf("X Step size changed to: %.2f\n", x_current_step);
}

// Y Step button event handler
void UITabControlJog::y_step_button_event_cb(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    y_current_step_index = index;
    y_current_step = UITheme::XY_STEP_VALUES[index];
    update_xy_step_display();
    update_y_step_button_styles();
    
    Serial.printf("Y Step size changed to: %.2f\n", y_current_step);
}

// Z Step button event handler
void UITabControlJog::z_step_button_event_cb(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    z_current_step_index = index;
    z_current_step = UITheme::Z_STEP_VALUES[index];
    update_z_step_display();
    update_z_step_button_styles();
    
    Serial.printf("Z Step size changed to: %.2f\n", z_current_step);
}

// Update the XY step display in the center button
void UITabControlJog::update_xy_step_display() {
    if (xy_step_display_label != nullptr && xy_feedrate_label != nullptr) {
        char feedrate_text[20];
        if (rapid_feed_enabled) {
            snprintf(feedrate_text, sizeof(feedrate_text), "%.0fR", rapidXYFeedValue());
        } else {
            snprintf(feedrate_text, sizeof(feedrate_text), "%s", lv_textarea_get_text(xy_feedrate_label));
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "X:%.2f Y:%.2f\nF:%s", x_current_step, y_current_step, feedrate_text);
        lv_label_set_text(xy_step_display_label, buf);
    }
}

// Update the Z step display between up/down buttons
void UITabControlJog::update_z_step_display() {
    if (z_step_display_label != nullptr && z_feedrate_label != nullptr) {
        char feedrate_text[20];
        if (rapid_feed_enabled) {
            snprintf(feedrate_text, sizeof(feedrate_text), "%.0fR", rapidZFeedValue());
        } else {
            snprintf(feedrate_text, sizeof(feedrate_text), "%s", lv_textarea_get_text(z_feedrate_label));
        }
        char buf[32];
        if (z_current_step < 0.1f) {
            snprintf(buf, sizeof(buf), "S:%.2f\nF:%s", z_current_step, feedrate_text);
        } else if (z_current_step < 1.0f) {
            snprintf(buf, sizeof(buf), "S:%.1f\nF:%s", z_current_step, feedrate_text);
        } else {
            snprintf(buf, sizeof(buf), "S:%.0f\nF:%s", z_current_step, feedrate_text);
        }
        lv_label_set_text(z_step_display_label, buf);
    }
}

// Update XY button styles to highlight the selected step
void UITabControlJog::update_x_step_button_styles() {
    for (int i = 0; i < UITheme::XY_STEP_COUNT; i++) {
        if (x_step_buttons[i] != nullptr) {
            if (i == x_current_step_index) {
                lv_obj_set_style_bg_color(x_step_buttons[i], UITheme::ACCENT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(x_step_buttons[i], UITheme::ACCENT_PRIMARY_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
            } else {
                lv_obj_set_style_bg_color(x_step_buttons[i], UITheme::BG_BUTTON, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(x_step_buttons[i], UITheme::BORDER_LIGHT, LV_PART_MAIN | LV_STATE_PRESSED);
            }
        }
    }
}

// Update Y button styles to highlight the selected step
void UITabControlJog::update_y_step_button_styles() {
    for (int i = 0; i < UITheme::XY_STEP_COUNT; i++) {
        if (y_step_buttons[i] != nullptr) {
            if (i == y_current_step_index) {
                lv_obj_set_style_bg_color(y_step_buttons[i], UITheme::ACCENT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(y_step_buttons[i], UITheme::ACCENT_PRIMARY_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
            } else {
                lv_obj_set_style_bg_color(y_step_buttons[i], UITheme::BG_BUTTON, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(y_step_buttons[i], UITheme::BORDER_LIGHT, LV_PART_MAIN | LV_STATE_PRESSED);
            }
        }
    }
}
// Update Z button styles to highlight the selected step
void UITabControlJog::update_z_step_button_styles() {
    for (int i = 0; i < UITheme::Z_STEP_COUNT; i++) {
        if (z_step_buttons[i] != nullptr) {
            if (i == z_current_step_index) {
                lv_obj_set_style_bg_color(z_step_buttons[i], UITheme::ACCENT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(z_step_buttons[i], UITheme::ACCENT_PRIMARY_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
            } else {
                lv_obj_set_style_bg_color(z_step_buttons[i], UITheme::BG_BUTTON, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(z_step_buttons[i], UITheme::BORDER_LIGHT, LV_PART_MAIN | LV_STATE_PRESSED);
            }
        }
    }
}

void UITabControlJog::encoder_bind_button_event_cb(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    if (index == UICommon::getEncoderBindAxis()) {
        UICommon::setEncoderBindEnabled(!UICommon::isEncoderBindEnabled(), true);
    } else {
        UICommon::setEncoderBindAxis(index, true);
    }
    update_encoder_bind_button_styles();
    reset_override_encoder_count();
}

void UITabControlJog::update_encoder_bind_button_styles() {
    int axis = UICommon::getEncoderBindAxis();
    const bool enabled = UICommon::isEncoderBindEnabled();
    for (int i = 0; i < 3; ++i) {
        if (!encoder_bind_buttons[i]) {
            continue;
        }
        if (i == axis) {
            if (enabled) {
                lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::ACCENT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::ACCENT_PRIMARY_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
                lv_obj_set_style_border_width(encoder_bind_buttons[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::BG_BUTTON, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::BORDER_LIGHT, LV_PART_MAIN | LV_STATE_PRESSED);
                lv_obj_set_style_border_width(encoder_bind_buttons[i], 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(encoder_bind_buttons[i], UITheme::ACCENT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        } else {
            lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::BG_BUTTON, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::BORDER_LIGHT, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_width(encoder_bind_buttons[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void UITabControlJog::reset_override_encoder_count() {
    int32_t count = 0;
    if (UsbHostManager::getSingleDeviceCount(&count)) {
        last_override_count = count;
        last_override_count_valid = true;
    } else {
        last_override_count_valid = false;
    }
}

void UITabControlJog::soft_limits_button_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    loadSoftLimitsFromConfig();
    syncSoftLimitsUI();
}

void UITabControlJog::soft_limits_close_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    hideSoftLimitsKeyboard();
}

UITabControlJog::SoftLimitMode UITabControlJog::sliderValueToSoftLimitMode(lv_obj_t *slider) {
    if (!slider) {
        return SOFT_LIMIT_MODE_OFF;
    }
    const int value = lv_slider_get_value(slider);
    if (value <= -1) {
        return SOFT_LIMIT_MODE_TRACK;
    }
    if (value >= 1) {
        return SOFT_LIMIT_MODE_ON;
    }
    return SOFT_LIMIT_MODE_OFF;
}

void UITabControlJog::setSliderFromSoftLimitMode(lv_obj_t *slider, SoftLimitMode mode) {
    if (!slider) {
        return;
    }
    lv_slider_set_value(slider, static_cast<int16_t>(mode), LV_ANIM_OFF);
}

void UITabControlJog::updateSoftLimitModeLabel(char axis, SoftLimitMode mode) {
    const char *text = "Off";
    if (mode == SOFT_LIMIT_MODE_TRACK) {
        text = "Track";
    } else if (mode == SOFT_LIMIT_MODE_ON) {
        text = "On";
    }

    lv_obj_t *label = nullptr;
    if (axis == 'X' || axis == 'x') {
        label = soft_limits_mode_label_x;
    } else if (axis == 'Y' || axis == 'y') {
        label = soft_limits_mode_label_y;
    } else if (axis == 'Z' || axis == 'z') {
        label = soft_limits_mode_label_z;
    }
    if (label) {
        lv_label_set_text(label, text);
    }
}

void UITabControlJog::soft_limit_mode_slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    if (!slider) {
        return;
    }

    SoftLimitMode mode = sliderValueToSoftLimitMode(slider);
    setSliderFromSoftLimitMode(slider, mode);

    const char axis = static_cast<char>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    const bool connected = CommManager::isConnected();
    const FluidNCStatus &status = CommManager::getStatus();

    if (axis == 'X' || axis == 'x') {
        soft_limit_x_mode = mode;
        soft_limit_x_enabled = (mode == SOFT_LIMIT_MODE_ON);
        soft_limit_x_learn_initialized = false;
        applySoftLimitsToComm();
        updateSoftLimitModeLabel('X', mode);
        return;
    }

    if (axis == 'Y' || axis == 'y') {
        soft_limit_y_mode = mode;
        soft_limit_y_enabled = (mode == SOFT_LIMIT_MODE_ON);
        soft_limit_y_learn_initialized = false;
        applySoftLimitsToComm();
        updateSoftLimitModeLabel('Y', mode);
        return;
    }

    if (axis == 'Z' || axis == 'z') {
        soft_limit_z_mode = mode;
        soft_limit_z_enabled = (mode == SOFT_LIMIT_MODE_ON);
        soft_limit_z_learn_initialized = false;
        applySoftLimitsToComm();
        updateSoftLimitModeLabel('Z', mode);
    }
}

void UITabControlJog::soft_limits_save_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    storeSoftLimitsFromUI();
    saveSoftLimitsToConfig();
    applySoftLimitsToComm();
    hideSoftLimitsKeyboard();
}

void UITabControlJog::soft_limits_textarea_focused_event_cb(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    char axis = 'X';
    if (ta == soft_limits_y_min_ta || ta == soft_limits_y_max_ta) {
        axis = 'Y';
    } else if (ta == soft_limits_z_min_ta || ta == soft_limits_z_max_ta) {
        axis = 'Z';
    }
    setActiveNumericTextarea(ta, axis);
    UICommon::registerKeyboardTarget(ta, UITabControlJog::showSoftLimitsKeyboard, UITabControlJog::hideSoftLimitsKeyboard);
    if (UICommon::isOnScreenKeyboardEnabled()) {
        showSoftLimitsKeyboard(ta);
    } else {
        hideSoftLimitsKeyboard();
    }
}

void UITabControlJog::jog_feed_textarea_focused_event_cb(lv_event_t *e) {
    lv_obj_t *ta = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const char axis = static_cast<char>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    setActiveNumericTextarea(ta, axis);
    UICommon::registerKeyboardTarget(ta, UITabControlJog::showJogKeyboard, UITabControlJog::hideJogKeyboard);
    if (UICommon::isOnScreenKeyboardEnabled()) {
        showJogKeyboard(ta);
    } else {
        hideJogKeyboard();
    }
}

void UITabControlJog::jog_feed_textarea_defocused_event_cb(lv_event_t *e) {
    lv_obj_t *ta = static_cast<lv_obj_t *>(lv_event_get_target(e));
    UICommon::clearKeyboardTarget(ta);
}

void UITabControlJog::showJogKeyboard(lv_obj_t *ta) {
    if (!jog_keyboard) {
        jog_keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(jog_keyboard, SCREEN_WIDTH, UI_SCALE_Y(220));
        lv_obj_align(jog_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(jog_keyboard, &lv_font_montserrat_20, 0);
        lv_keyboard_set_mode(jog_keyboard, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_add_event_cb(jog_keyboard, [](lv_event_t *e) { UITabControlJog::hideJogKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(jog_keyboard, [](lv_event_t *e) { UITabControlJog::hideJogKeyboard(); }, LV_EVENT_CANCEL, nullptr);
    }
    lv_keyboard_set_textarea(jog_keyboard, ta);
    lv_obj_clear_flag(jog_keyboard, LV_OBJ_FLAG_HIDDEN);
}

void UITabControlJog::hideJogKeyboard() {
    if (jog_keyboard) {
        lv_obj_add_flag(jog_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void UITabControlJog::showSoftLimitsKeyboard(lv_obj_t *ta) {
    soft_limits_active_ta = ta;
    if (!soft_limits_keyboard) {
        soft_limits_keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(soft_limits_keyboard, SCREEN_WIDTH, UI_SCALE_Y(220));
        lv_obj_align(soft_limits_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(soft_limits_keyboard, &lv_font_montserrat_20, 0);
        lv_keyboard_set_mode(soft_limits_keyboard, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_add_event_cb(soft_limits_keyboard, [](lv_event_t *e) { UITabControlJog::hideSoftLimitsKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(soft_limits_keyboard, [](lv_event_t *e) { UITabControlJog::hideSoftLimitsKeyboard(); }, LV_EVENT_CANCEL, nullptr);
    }
    lv_keyboard_set_textarea(soft_limits_keyboard, ta);
    lv_obj_clear_flag(soft_limits_keyboard, LV_OBJ_FLAG_HIDDEN);
}

void UITabControlJog::soft_limits_textarea_changed_event_cb(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    if (!ta || ta != soft_limits_active_ta) {
        return;
    }
    if (ta == soft_limits_x_min_ta || ta == soft_limits_x_max_ta) {
        normalize_min_max_pair(soft_limits_x_min_ta, soft_limits_x_max_ta, ta);
        float min_v = 0.0f;
        float max_v = 0.0f;
        if (parse_textarea_float(soft_limits_x_min_ta, min_v) &&
            parse_textarea_float(soft_limits_x_max_ta, max_v)) {
            soft_limit_x_min = min_v;
            soft_limit_x_max = max_v;
            if (soft_limit_x_mode == SOFT_LIMIT_MODE_ON) {
                applySoftLimitsToComm();
            }
        }
        return;
    }
    if (ta == soft_limits_y_min_ta || ta == soft_limits_y_max_ta) {
        normalize_min_max_pair(soft_limits_y_min_ta, soft_limits_y_max_ta, ta);
        float min_v = 0.0f;
        float max_v = 0.0f;
        if (parse_textarea_float(soft_limits_y_min_ta, min_v) &&
            parse_textarea_float(soft_limits_y_max_ta, max_v)) {
            soft_limit_y_min = min_v;
            soft_limit_y_max = max_v;
            if (soft_limit_y_mode == SOFT_LIMIT_MODE_ON) {
                applySoftLimitsToComm();
            }
        }
        return;
    }
    if (ta == soft_limits_z_min_ta || ta == soft_limits_z_max_ta) {
        normalize_min_max_pair(soft_limits_z_min_ta, soft_limits_z_max_ta, ta);
        float min_v = 0.0f;
        float max_v = 0.0f;
        if (parse_textarea_float(soft_limits_z_min_ta, min_v) &&
            parse_textarea_float(soft_limits_z_max_ta, max_v)) {
            soft_limit_z_min = min_v;
            soft_limit_z_max = max_v;
            if (soft_limit_z_mode == SOFT_LIMIT_MODE_ON) {
                applySoftLimitsToComm();
            }
        }
    }
}

void UITabControlJog::hideSoftLimitsKeyboard() {
    if (soft_limits_keyboard) {
        lv_obj_add_flag(soft_limits_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void UITabControlJog::loadSoftLimitsFromConfig() {
    MachineConfig config;
    if (MachineConfigManager::getSelectedMachine(config)) {
        soft_limit_x_mode = static_cast<SoftLimitMode>(config.soft_limit_x_mode);
        soft_limit_y_mode = static_cast<SoftLimitMode>(config.soft_limit_y_mode);
        soft_limit_z_mode = static_cast<SoftLimitMode>(config.soft_limit_z_mode);
        if (soft_limit_x_mode != SOFT_LIMIT_MODE_TRACK &&
            soft_limit_x_mode != SOFT_LIMIT_MODE_OFF &&
            soft_limit_x_mode != SOFT_LIMIT_MODE_ON) {
            soft_limit_x_mode = config.soft_limit_x_enabled ? SOFT_LIMIT_MODE_ON : SOFT_LIMIT_MODE_OFF;
        }
        if (soft_limit_y_mode != SOFT_LIMIT_MODE_TRACK &&
            soft_limit_y_mode != SOFT_LIMIT_MODE_OFF &&
            soft_limit_y_mode != SOFT_LIMIT_MODE_ON) {
            soft_limit_y_mode = config.soft_limit_y_enabled ? SOFT_LIMIT_MODE_ON : SOFT_LIMIT_MODE_OFF;
        }
        if (soft_limit_z_mode != SOFT_LIMIT_MODE_TRACK &&
            soft_limit_z_mode != SOFT_LIMIT_MODE_OFF &&
            soft_limit_z_mode != SOFT_LIMIT_MODE_ON) {
            soft_limit_z_mode = config.soft_limit_z_enabled ? SOFT_LIMIT_MODE_ON : SOFT_LIMIT_MODE_OFF;
        }
        soft_limit_x_enabled = (soft_limit_x_mode == SOFT_LIMIT_MODE_ON);
        soft_limit_y_enabled = (soft_limit_y_mode == SOFT_LIMIT_MODE_ON);
        soft_limit_z_enabled = (soft_limit_z_mode == SOFT_LIMIT_MODE_ON);
        soft_limit_x_min = config.soft_limit_x_min;
        soft_limit_x_max = config.soft_limit_x_max;
        soft_limit_y_min = config.soft_limit_y_min;
        soft_limit_y_max = config.soft_limit_y_max;
        soft_limit_z_min = config.soft_limit_z_min;
        soft_limit_z_max = config.soft_limit_z_max;
        if (soft_limit_x_min > soft_limit_x_max) {
            soft_limit_x_max = soft_limit_x_min;
        }
        if (soft_limit_y_min > soft_limit_y_max) {
            soft_limit_y_max = soft_limit_y_min;
        }
        if (soft_limit_z_min > soft_limit_z_max) {
            soft_limit_z_max = soft_limit_z_min;
        }

        soft_limit_x_learn_initialized = false;
        soft_limit_y_learn_initialized = false;
        soft_limit_z_learn_initialized = false;
    }
}

void UITabControlJog::saveSoftLimitsToConfig() {
    int index = MachineConfigManager::getSelectedMachineIndex();
    if (index < 0 || index >= MAX_MACHINES) {
        return;
    }
    MachineConfig config;
    if (!MachineConfigManager::getMachine(index, config)) {
        return;
    }
    config.soft_limit_x_mode = static_cast<int8_t>(soft_limit_x_mode);
    config.soft_limit_y_mode = static_cast<int8_t>(soft_limit_y_mode);
    config.soft_limit_z_mode = static_cast<int8_t>(soft_limit_z_mode);
    config.soft_limit_x_enabled = soft_limit_x_enabled;
    config.soft_limit_y_enabled = soft_limit_y_enabled;
    config.soft_limit_z_enabled = soft_limit_z_enabled;
    config.soft_limit_x_min = soft_limit_x_min;
    config.soft_limit_x_max = soft_limit_x_max;
    config.soft_limit_y_min = soft_limit_y_min;
    config.soft_limit_y_max = soft_limit_y_max;
    config.soft_limit_z_min = soft_limit_z_min;
    config.soft_limit_z_max = soft_limit_z_max;
    MachineConfigManager::saveMachine(index, config);
}

void UITabControlJog::applySoftLimitsToComm() {
    CommManager::setJogSoftLimits(soft_limit_x_enabled,
                                  soft_limit_y_enabled,
                                  soft_limit_z_enabled,
                                  soft_limit_x_min,
                                  soft_limit_x_max,
                                  soft_limit_y_min,
                                  soft_limit_y_max,
                                  soft_limit_z_min,
                                  soft_limit_z_max);
}

void UITabControlJog::updateSoftLimitLearnTracking(const FluidNCStatus &status) {
    bool changed = false;

    if (soft_limit_x_mode == SOFT_LIMIT_MODE_TRACK) {
        if (status.wpos_x < soft_limit_x_min) {
            soft_limit_x_min = status.wpos_x;
            changed = true;
        }
        if (status.wpos_x > soft_limit_x_max) {
            soft_limit_x_max = status.wpos_x;
            changed = true;
        }
    }

    if (soft_limit_y_mode == SOFT_LIMIT_MODE_TRACK) {
        if (status.wpos_y < soft_limit_y_min) {
            soft_limit_y_min = status.wpos_y;
            changed = true;
        }
        if (status.wpos_y > soft_limit_y_max) {
            soft_limit_y_max = status.wpos_y;
            changed = true;
        }
    }

    if (soft_limit_z_mode == SOFT_LIMIT_MODE_TRACK) {
        if (status.wpos_z < soft_limit_z_min) {
            soft_limit_z_min = status.wpos_z;
            changed = true;
        }
        if (status.wpos_z > soft_limit_z_max) {
            soft_limit_z_max = status.wpos_z;
            changed = true;
        }
    }

    if (changed) {
        syncSoftLimitsUI();
    }
}

void UITabControlJog::syncSoftLimitsUI() {
    setSliderFromSoftLimitMode(soft_limits_mode_slider_x, soft_limit_x_mode);
    setSliderFromSoftLimitMode(soft_limits_mode_slider_y, soft_limit_y_mode);
    setSliderFromSoftLimitMode(soft_limits_mode_slider_z, soft_limit_z_mode);
    updateSoftLimitModeLabel('X', soft_limit_x_mode);
    updateSoftLimitModeLabel('Y', soft_limit_y_mode);
    updateSoftLimitModeLabel('Z', soft_limit_z_mode);

    char buf[32];
    if (soft_limits_x_min_ta) {
        snprintf(buf, sizeof(buf), "%.3f", soft_limit_x_min);
        lv_textarea_set_text(soft_limits_x_min_ta, buf);
    }
    if (soft_limits_x_max_ta) {
        snprintf(buf, sizeof(buf), "%.3f", soft_limit_x_max);
        lv_textarea_set_text(soft_limits_x_max_ta, buf);
    }
    if (soft_limits_y_min_ta) {
        snprintf(buf, sizeof(buf), "%.3f", soft_limit_y_min);
        lv_textarea_set_text(soft_limits_y_min_ta, buf);
    }
    if (soft_limits_y_max_ta) {
        snprintf(buf, sizeof(buf), "%.3f", soft_limit_y_max);
        lv_textarea_set_text(soft_limits_y_max_ta, buf);
    }
    if (soft_limits_z_min_ta) {
        snprintf(buf, sizeof(buf), "%.3f", soft_limit_z_min);
        lv_textarea_set_text(soft_limits_z_min_ta, buf);
    }
    if (soft_limits_z_max_ta) {
        snprintf(buf, sizeof(buf), "%.3f", soft_limit_z_max);
        lv_textarea_set_text(soft_limits_z_max_ta, buf);
    }
}

void UITabControlJog::storeSoftLimitsFromUI() {
    soft_limit_x_mode = sliderValueToSoftLimitMode(soft_limits_mode_slider_x);
    soft_limit_y_mode = sliderValueToSoftLimitMode(soft_limits_mode_slider_y);
    soft_limit_z_mode = sliderValueToSoftLimitMode(soft_limits_mode_slider_z);
    soft_limit_x_enabled = (soft_limit_x_mode == SOFT_LIMIT_MODE_ON);
    soft_limit_y_enabled = (soft_limit_y_mode == SOFT_LIMIT_MODE_ON);
    soft_limit_z_enabled = (soft_limit_z_mode == SOFT_LIMIT_MODE_ON);

    if (soft_limits_x_min_ta) soft_limit_x_min = strtof(lv_textarea_get_text(soft_limits_x_min_ta), nullptr);
    if (soft_limits_x_max_ta) soft_limit_x_max = strtof(lv_textarea_get_text(soft_limits_x_max_ta), nullptr);
    if (soft_limits_y_min_ta) soft_limit_y_min = strtof(lv_textarea_get_text(soft_limits_y_min_ta), nullptr);
    if (soft_limits_y_max_ta) soft_limit_y_max = strtof(lv_textarea_get_text(soft_limits_y_max_ta), nullptr);
    if (soft_limits_z_min_ta) soft_limit_z_min = strtof(lv_textarea_get_text(soft_limits_z_min_ta), nullptr);
    if (soft_limits_z_max_ta) soft_limit_z_max = strtof(lv_textarea_get_text(soft_limits_z_max_ta), nullptr);

    if (soft_limit_x_min > soft_limit_x_max) soft_limit_x_max = soft_limit_x_min;
    if (soft_limit_y_min > soft_limit_y_max) soft_limit_y_max = soft_limit_y_min;
    if (soft_limit_z_min > soft_limit_z_max) soft_limit_z_max = soft_limit_z_min;
}

void UITabControlJog::updateJogDebugInfoUI() {
    float predicted_x = 0.0f;
    float predicted_y = 0.0f;
    float predicted_z = 0.0f;
    const bool valid = CommManager::getJogPredictedWpos(predicted_x, predicted_y, predicted_z);
    const int32_t pending_cmds = CommManager::getJogPendingCommandCount();

    if (jog_predicted_wpos_label) {
        char buf[96];
        if (valid) {
            snprintf(buf, sizeof(buf), "Pred WPos\nX: %.3f\nY: %.3f\nZ: %.3f",
                     predicted_x, predicted_y, predicted_z);
        } else {
            snprintf(buf, sizeof(buf), "Pred WPos\nX: ---\nY: ---\nZ: ---");
        }
        lv_label_set_text(jog_predicted_wpos_label, buf);
    }

    if (jog_pending_cmd_count_label) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Pending Cmds: %ld", static_cast<long>(pending_cmds));
        lv_label_set_text(jog_pending_cmd_count_label, buf);
    }
}


// XY Feedrate adjustment button event handler
void UITabControlJog::xy_feedrate_adj_event_cb(lv_event_t *e) {
    int adjustment = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (xy_feedrate_label != nullptr) {
        float current_value = get_jog_feed_value(xy_feedrate_label, xy_current_feed, 10000.0f);
        float new_value = clamp_jog_feed(current_value + static_cast<float>(adjustment), 10000.0f);
        set_jog_feed_text(xy_feedrate_label, new_value);
        xy_current_feed = new_value;
        
        // Update the center display to show new feedrate
        update_xy_step_display();
        
        Serial.printf("XY Feedrate adjusted by %d to: %.3f mm/min\n", adjustment, new_value);
    }
}

// Z Feedrate adjustment button event handler
void UITabControlJog::z_feedrate_adj_event_cb(lv_event_t *e) {
    int adjustment = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (z_feedrate_label != nullptr) {
        float current_value = get_jog_feed_value(z_feedrate_label, z_current_feed, 5000.0f);
        float new_value = clamp_jog_feed(current_value + static_cast<float>(adjustment), 5000.0f);
        set_jog_feed_text(z_feedrate_label, new_value);
        z_current_feed = new_value;
        
        // Update the center display to show new feedrate
        update_z_step_display();
        
        Serial.printf("Z Feedrate adjusted by %d to: %.3f mm/min\n", adjustment, new_value);
    }
}

// XY Jog button event handler
void UITabControlJog::xy_jog_button_event_cb(lv_event_t *e) {
    if (!CommManager::isConnected()) {
        Serial.println("[Jog] Not connected to FluidNC");
        return;
    }

    int button_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Get current feedrate
    float feedrate = effectiveXYFeedValue();
    
    // Map button index to X/Y movement
    // 0=NW, 1=N, 2=NE, 3=W, 4=center(skip), 5=E, 6=SW, 7=S, 8=SE
    float x_move = 0, y_move = 0;
    
    switch (button_index) {
        case 0:  // NW (-X, +Y)
            x_move = -x_current_step;
            y_move = y_current_step;
            break;
        case 1:  // N (+Y)
            y_move = y_current_step;
            break;
        case 2:  // NE (+X, +Y)
            x_move = x_current_step;
            y_move = y_current_step;
            break;
        case 3:  // W (-X)
            x_move = -x_current_step;
            break;
        case 5:  // E (+X)
            x_move = x_current_step;
            break;
        case 6:  // SW (-X, -Y)
            x_move = -x_current_step;
            y_move = -y_current_step;
            break;
        case 7:  // S (-Y)
            y_move = -y_current_step;
            break;
        case 8:  // SE (+X, -Y)
            x_move = x_current_step;
            y_move = -y_current_step;
            break;
    }

    CommManager::sendJogRelative(x_move, y_move, 0.0f, feedrate);
}

// Z Jog button event handler
void UITabControlJog::z_jog_button_event_cb(lv_event_t *e) {
    if (!CommManager::isConnected()) {
        Serial.println("[Jog] Not connected to FluidNC");
        return;
    }
    
    int direction = (int)(intptr_t)lv_event_get_user_data(e);  // +1 for up, -1 for down
    
    // Get current feedrate
    float feedrate = effectiveZFeedValue();
    
    // Calculate Z movement
    float z_move = z_current_step * direction;

    CommManager::sendJogRelative(0.0f, 0.0f, z_move, feedrate);
}

// Draw octagon shape for stop button
void UITabControlJog::draw_octagon_event_cb(lv_event_t *e) {
    lv_obj_t *obj = (lv_obj_t*)lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    
    // Get object coordinates
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    
    int size = lv_area_get_width(&coords);
    if (size <= 0) {
        return;
    }
    int border_width = size / 23;
    if (border_width < 1) {
        border_width = 1;
    }
    
    // Draw outer white octagon (border)
    int cut_outer = (size * 2) / 7;
    lv_draw_line_dsc_t white_line_dsc;
    lv_draw_line_dsc_init(&white_line_dsc);
    white_line_dsc.color = lv_color_white();
    white_line_dsc.width = 1;
    white_line_dsc.opa = LV_OPA_COVER;
    
    for (int y = 0; y < size; y++) {
        int x_start = -1, x_end = -1;
        
        if (y < cut_outer) {
            x_start = cut_outer - y;
            x_end = size - cut_outer + y;
        } else if (y < size - cut_outer) {
            x_start = 0;
            x_end = size;
        } else {
            x_start = y - (size - cut_outer);
            x_end = size - (y - (size - cut_outer));
        }
        
        if (x_start >= 0 && x_end >= 0) {
            white_line_dsc.p1.x = coords.x1 + x_start;
            white_line_dsc.p1.y = coords.y1 + y;
            white_line_dsc.p2.x = coords.x1 + x_end - 1;
            white_line_dsc.p2.y = coords.y1 + y;
            lv_draw_line(layer, &white_line_dsc);
        }
    }
    
    // Draw inner red octagon (smaller by border_width on all sides)
    int cut_inner = cut_outer - border_width;
    int offset = border_width;
    int inner_size = size - (border_width * 2);
    
    lv_draw_line_dsc_t red_line_dsc;
    lv_draw_line_dsc_init(&red_line_dsc);
    red_line_dsc.color = lv_color_hex(0xCC0000);  // Red like stop sign
    red_line_dsc.width = 1;
    red_line_dsc.opa = LV_OPA_COVER;
    
    for (int y = 0; y < inner_size; y++) {
        int x_start = -1, x_end = -1;
        
        if (y < cut_inner) {
            x_start = cut_inner - y;
            x_end = inner_size - cut_inner + y;
        } else if (y < inner_size - cut_inner) {
            x_start = 0;
            x_end = inner_size;
        } else {
            x_start = y - (inner_size - cut_inner);
            x_end = inner_size - (y - (inner_size - cut_inner));
        }
        
        if (x_start >= 0 && x_end >= 0) {
            red_line_dsc.p1.x = coords.x1 + offset + x_start;
            red_line_dsc.p1.y = coords.y1 + offset + y;
            red_line_dsc.p2.x = coords.x1 + offset + x_end - 1;
            red_line_dsc.p2.y = coords.y1 + offset + y;
            lv_draw_line(layer, &red_line_dsc);
        }
    }
}

// Cancel Jog event handler
void UITabControlJog::cancel_jog_event_cb(lv_event_t *e) {
    if (!CommManager::isConnected()) {
        Serial.println("[Jog] Not connected to FluidNC");
        return;
    }
    
    // Send jog cancel command (0x85 or Ctrl-U)
    CommManager::sendCommand("\x85");
    Serial.println("[Jog] Cancel jog command sent");
}

void UITabControlJog::encoderTimerCb(lv_timer_t *timer) {
    LV_UNUSED(timer);

    UICommon::updateEncoderBindVisibility();
    if (encoder_bind_container) {
        update_encoder_bind_button_styles();
        bool should_show = UICommon::isEncoderBindVisible();
        bool is_hidden = lv_obj_has_flag(encoder_bind_container, LV_OBJ_FLAG_HIDDEN);
        if (should_show && is_hidden) {
            lv_obj_clear_flag(encoder_bind_container, LV_OBJ_FLAG_HIDDEN);
            reset_override_encoder_count();
        } else if (!should_show && !is_hidden) {
            lv_obj_add_flag(encoder_bind_container, LV_OBJ_FLAG_HIDDEN);
            last_override_count_valid = false;
            for (size_t i = 0; i < 3; ++i) {
                last_encoder_counts[i] = get_encoder_value(i);
            }
        }
    }

    if (parent_tab && lv_obj_has_flag(parent_tab, LV_OBJ_FLAG_HIDDEN)) {
        clearActiveNumericTextarea(nullptr);
        for (size_t i = 0; i < 3; ++i) {
            last_encoder_counts[i] = get_encoder_value(i);
        }
        if (UICommon::isEncoderBindVisible()) {
            reset_override_encoder_count();
        }
        return;
    }

    if (CommManager::isConnected()) {
        updateSoftLimitLearnTracking(CommManager::getStatus());
    }
    updateJogDebugInfoUI();

    if (isFieldNavigationActive()) {
        int32_t delta = 0;
        if (UICommon::isEncoderBindVisible()) {
            int32_t count = 0;
            if (UsbHostManager::getSingleDeviceCount(&count)) {
                if (!last_override_count_valid) {
                    last_override_count = count;
                    last_override_count_valid = true;
                }
                delta = count - last_override_count;
                last_override_count = count;
                UICommon::maybeSendEncoderBindDisplay(false);
            }
        }

        if (delta == 0) {
            int axis_idx = UICommon::getEncoderBindAxis();
            if (axis_idx < 0 || axis_idx > 2) {
                axis_idx = 0;
            }
            int16_t count = get_encoder_value(static_cast<size_t>(axis_idx));
            delta = count - last_encoder_counts[axis_idx];
        }

        for (size_t i = 0; i < 3; ++i) {
            int16_t count = get_encoder_value(i);
            last_encoder_counts[i] = count;
        }

        if (delta != 0) {
            const int direction = (delta > 0) ? 1 : -1;
            for (int32_t step = 0; step < std::abs(delta); ++step) {
                navigateActiveFieldSelection(direction);
            }
        }
        return;
    }

    if (active_navigable_obj) {
        if (!lv_obj_is_valid(active_navigable_obj)) {
            clearActiveNumericTextarea(nullptr);
        } else {
            int32_t delta = 0;
            if (UICommon::isEncoderBindVisible()) {
                int32_t count = 0;
                if (UsbHostManager::getSingleDeviceCount(&count)) {
                    if (!last_override_count_valid) {
                        last_override_count = count;
                        last_override_count_valid = true;
                    }
                    delta = count - last_override_count;
                    last_override_count = count;
                    UICommon::maybeSendEncoderBindDisplay(false);
                }
            }

            if (delta == 0) {
                const int axis_idx = axis_index_from_hint(active_numeric_axis);
                int16_t count = get_encoder_value(static_cast<size_t>(axis_idx));
                delta = count - last_encoder_counts[axis_idx];
            }

            for (size_t i = 0; i < 3; ++i) {
                int16_t count = get_encoder_value(i);
                last_encoder_counts[i] = count;
            }

            if (delta != 0) {
                NavigableFieldEntry entry{};
                if (!findNavigableEntry(active_navigable_obj, &entry)) {
                    clearActiveNumericTextarea(nullptr);
                    return;
                }

                if (entry.type == NavigableFieldType::Textarea) {
                    const float step = (active_numeric_axis == 'R' || active_numeric_axis == 'r') ? 1.0f :
                                       (active_numeric_axis == 'Y' || active_numeric_axis == 'y') ? y_current_step :
                                       (active_numeric_axis == 'Z' || active_numeric_axis == 'z') ? z_current_step :
                                                                                                      x_current_step;
                    const char *text = lv_textarea_get_text(active_numeric_ta);
                    double value = (text && text[0] != '\0') ? strtod(text, nullptr) : 0.0;
                    value += static_cast<double>(delta) * static_cast<double>(step);

                    if (active_numeric_ta == xy_feedrate_label) {
                        value = static_cast<double>(clamp_jog_feed(static_cast<float>(value), 10000.0f));
                        xy_current_feed = static_cast<float>(value);
                    } else if (active_numeric_ta == z_feedrate_label) {
                        value = static_cast<double>(clamp_jog_feed(static_cast<float>(value), 5000.0f));
                        z_current_feed = static_cast<float>(value);
                    } else if (active_numeric_axis == 'R' || active_numeric_axis == 'r') {
                        if (value < 1.0) {
                            value = 1.0;
                        } else if (value > 999.0) {
                            value = 999.0;
                        }
                    }

                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.4f", value);
                    trim_float_string(buf);
                    lv_textarea_set_text(active_numeric_ta, buf);
                } else if (entry.type == NavigableFieldType::Switch) {
                    if (delta > 0) {
                        lv_obj_add_state(active_navigable_obj, LV_STATE_CHECKED);
                    } else {
                        lv_obj_remove_state(active_navigable_obj, LV_STATE_CHECKED);
                    }
                    lv_obj_send_event(active_navigable_obj, LV_EVENT_VALUE_CHANGED, nullptr);
                } else if (entry.type == NavigableFieldType::Slider) {
                    const int32_t min_v = lv_slider_get_min_value(active_navigable_obj);
                    const int32_t max_v = lv_slider_get_max_value(active_navigable_obj);
                    int32_t value = lv_slider_get_value(active_navigable_obj);
                    value += delta;
                    if (value < min_v) value = min_v;
                    if (value > max_v) value = max_v;
                    lv_slider_set_value(active_navigable_obj, static_cast<int32_t>(value), LV_ANIM_OFF);
                    lv_obj_send_event(active_navigable_obj, LV_EVENT_VALUE_CHANGED, nullptr);
                } else if (entry.type == NavigableFieldType::Button) {
                    // Button selections are acted on through the click mapping.
                }
                PowerManager::onUserActivity();
            }
            return;
        }
    }

    if (!CommManager::isConnected()) {
        for (size_t i = 0; i < 3; ++i) {
            last_encoder_counts[i] = get_encoder_value(i);
        }
        if (UICommon::isEncoderBindVisible()) {
            reset_override_encoder_count();
            UICommon::maybeSendEncoderBindDisplay(false);
        }
        return;
    }

    const float xy_feedrate = effectiveXYFeedValue();
    const float z_feedrate = effectiveZFeedValue();

    struct AxisCmd {
        char axis;
        float step;
        float feedrate;
    };
    const AxisCmd axes[3] = {
        {'X', x_current_step, xy_feedrate},
        {'Y', y_current_step, xy_feedrate},
        {'Z', z_current_step, z_feedrate},
    };

    if (UICommon::isEncoderBindVisible()) {
        int32_t count = 0;
        if (UsbHostManager::getSingleDeviceCount(&count)) {
            if (!last_override_count_valid) {
                last_override_count = count;
                last_override_count_valid = true;
            }
            int32_t delta = count - last_override_count;
            last_override_count = count;
            if (delta != 0) {
                if (UICommon::isEncoderBindEnabled()) {
                    const AxisCmd &axis = axes[UICommon::getEncoderBindAxis()];
                    float move = axis.step * static_cast<float>(delta);
                    CommManager::sendJogRelativeAxis(axis.axis, move, static_cast<float>(axis.feedrate));
                    PowerManager::onUserActivity();
                }
            }
            UICommon::maybeSendEncoderBindDisplay(false);
            return;
        }
    }

    for (size_t i = 0; i < 3; ++i) {
        int16_t count = get_encoder_value(i);
        int16_t delta = count - last_encoder_counts[i];
        last_encoder_counts[i] = count;
        if (delta == 0) {
            continue;
        }

        float move = axes[i].step * static_cast<float>(delta);
        CommManager::sendJogRelativeAxis(axes[i].axis, move, static_cast<float>(axes[i].feedrate));
        PowerManager::onUserActivity();
    }
}
