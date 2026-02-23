#include "ui/tabs/control/ui_tab_control_jog.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
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
lv_obj_t *UITabControlJog::encoder_bind_container = nullptr;
lv_obj_t *UITabControlJog::encoder_bind_buttons[3] = {nullptr, nullptr, nullptr};
lv_timer_t *UITabControlJog::encoder_timer = nullptr;
lv_obj_t *UITabControlJog::soft_limits_button = nullptr;
lv_obj_t *UITabControlJog::soft_limits_panel = nullptr;
lv_obj_t *UITabControlJog::soft_limits_keyboard = nullptr;
lv_obj_t *UITabControlJog::soft_limits_active_ta = nullptr;
lv_obj_t *UITabControlJog::active_numeric_ta = nullptr;
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
int16_t UITabControlJog::last_encoder_counts[3] = {0, 0, 0};
int32_t UITabControlJog::last_override_count = 0;
bool UITabControlJog::last_override_count_valid = false;
float UITabControlJog::x_current_step = 0.01f;      // Will be loaded from settings
float UITabControlJog::y_current_step = 0.01f;      // Will be loaded from settings
float UITabControlJog::z_current_step = 0.01f;       // Will be loaded from settings
int UITabControlJog::x_current_step_index = 0;      // Will be recalculated
int UITabControlJog::y_current_step_index = 0;      // Will be recalculated
int UITabControlJog::z_current_step_index = 0;      // Will be recalculated
int UITabControlJog::xy_current_feed = 300;        // Will be loaded from settings
int UITabControlJog::z_current_feed = 100;         // Will be loaded from settings
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
    
    // XY Feed rate control
    lv_obj_t *xy_feed_label = lv_label_create(tab);
    lv_label_set_text(xy_feed_label, "XY Feed:");
    lv_obj_set_style_text_font(xy_feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(xy_feed_label, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(xy_feed_label, UI_SCALE_X(5), UI_SCALE_Y(210));
    
    // XY Feedrate value (plain text label) - load from settings
    xy_feedrate_label = lv_label_create(tab);
    char xy_feed_buf[16];
    snprintf(xy_feed_buf, sizeof(xy_feed_buf), "%d", xy_current_feed);
    lv_label_set_text(xy_feedrate_label, xy_feed_buf);
    lv_obj_set_style_text_font(xy_feedrate_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(xy_feedrate_label, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(xy_feedrate_label, UI_SCALE_X(75), UI_SCALE_Y(210));
    
    // Now update XY step display (after feedrate label exists)
    update_xy_step_display();
    
    lv_obj_t *xy_feed_unit = lv_label_create(tab);
    lv_label_set_text(xy_feed_unit, "mm/min");
    lv_obj_set_style_text_font(xy_feed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(xy_feed_unit, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(xy_feed_unit, UI_SCALE_X(115), UI_SCALE_Y(210));
    
    // XY Feedrate adjustment buttons - all on one line: -1000, -100, +100, +1000
    lv_obj_t *btn_xy_minus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_minus1000, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_minus1000, UI_SCALE_X(5), UI_SCALE_Y(230));
    lv_obj_add_event_cb(btn_xy_minus1000, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1000);
    lv_obj_t *lbl_xy_minus1000 = lv_label_create(btn_xy_minus1000);
    lv_label_set_text(lbl_xy_minus1000, "-1000");
    lv_obj_set_style_text_font(lbl_xy_minus1000, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_minus1000);
    
    lv_obj_t *btn_xy_minus100 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_minus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_minus100, UI_SCALE_X(5+(1*60)), UI_SCALE_Y(230));
    lv_obj_add_event_cb(btn_xy_minus100, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-100);
    lv_obj_t *lbl_xy_minus100 = lv_label_create(btn_xy_minus100);
    lv_label_set_text(lbl_xy_minus100, "-100");
    lv_obj_set_style_text_font(lbl_xy_minus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_minus100);
    
    lv_obj_t *btn_xy_plus100 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_plus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_plus100, UI_SCALE_X(5+(2*60)), UI_SCALE_Y(230));
    lv_obj_add_event_cb(btn_xy_plus100, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)100);
    lv_obj_t *lbl_xy_plus100 = lv_label_create(btn_xy_plus100);
    lv_label_set_text(lbl_xy_plus100, "+100");
    lv_obj_set_style_text_font(lbl_xy_plus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_plus100);
    
    lv_obj_t *btn_xy_plus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_plus1000, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_plus1000, UI_SCALE_X(5+(3*60)), UI_SCALE_Y(230));
    lv_obj_add_event_cb(btn_xy_plus1000, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1000);
    lv_obj_t *lbl_xy_plus1000 = lv_label_create(btn_xy_plus1000);
    lv_label_set_text(lbl_xy_plus1000, "+1000");
    lv_obj_set_style_text_font(lbl_xy_plus1000, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_plus1000);
    
    z_step_display_label = nullptr;
    
    // Z Feed rate control
    lv_obj_t *z_feed_label = lv_label_create(tab);
    lv_label_set_text(z_feed_label, "Z Feed:");
    lv_obj_set_style_text_font(z_feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(z_feed_label, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(z_feed_label, UI_SCALE_X(5), UI_SCALE_Y(280));
    
    // Z Feedrate value (plain text label) - load from settings
    z_feedrate_label = lv_label_create(tab);
    char z_feed_buf[16];
    snprintf(z_feed_buf, sizeof(z_feed_buf), "%d", z_current_feed);
    lv_label_set_text(z_feedrate_label, z_feed_buf);
    lv_obj_set_style_text_font(z_feedrate_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(z_feedrate_label, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(z_feedrate_label, UI_SCALE_X(75), UI_SCALE_Y(280));
    
    // Now update Z step display (after feedrate label exists)
    update_z_step_display();
    
    lv_obj_t *z_feed_unit = lv_label_create(tab);
    lv_label_set_text(z_feed_unit, "mm/min");
    lv_obj_set_style_text_font(z_feed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(z_feed_unit, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_pos(z_feed_unit, UI_SCALE_X(115), UI_SCALE_Y(280));
    
    // Z Feedrate adjustment buttons - all on one line: -1000, -100, +100, +1000
    lv_obj_t *btn_z_minus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_z_minus1000, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_minus1000, UI_SCALE_X(5+(0*60)), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_minus1000, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1000);
    lv_obj_t *lbl_z_minus1000 = lv_label_create(btn_z_minus1000);
    lv_label_set_text(lbl_z_minus1000, "-1000");
    lv_obj_set_style_text_font(lbl_z_minus1000, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_minus1000);
    
    lv_obj_t *btn_z_minus100 = lv_button_create(tab);
    lv_obj_set_size(btn_z_minus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_minus100, UI_SCALE_X(5+(1*60)), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_minus100, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-100);
    lv_obj_t *lbl_z_minus100 = lv_label_create(btn_z_minus100);
    lv_label_set_text(lbl_z_minus100, "-100");
    lv_obj_set_style_text_font(lbl_z_minus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_minus100);
    
    lv_obj_t *btn_z_plus100 = lv_button_create(tab);
    lv_obj_set_size(btn_z_plus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_plus100, UI_SCALE_X(5+(2*60)), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_plus100, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)100);
    lv_obj_t *lbl_z_plus100 = lv_label_create(btn_z_plus100);
    lv_label_set_text(lbl_z_plus100, "+100");
    lv_obj_set_style_text_font(lbl_z_plus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_plus100);
    
    lv_obj_t *btn_z_plus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_z_plus1000, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_plus1000, UI_SCALE_X(5+(3*60)), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_plus1000, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1000);
    lv_obj_t *lbl_z_plus1000 = lv_label_create(btn_z_plus1000);
    lv_label_set_text(lbl_z_plus1000, "+1000");
    lv_obj_set_style_text_font(lbl_z_plus1000, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_plus1000);

    // ========== Cancel Jog Button (Upper Right) ==========
    // Create a container for the octagon stop button
    lv_obj_t *btn_cancel = lv_obj_create(tab);
    lv_obj_set_size(btn_cancel, UI_SCALE_X(70), UI_SCALE_Y(70));
    lv_obj_set_pos(btn_cancel, UI_SCALE_X(280), UI_SCALE_Y(275));
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

    // Soft limits
    soft_limits_panel = lv_obj_create(tab);
    lv_obj_set_size(soft_limits_panel, UI_SCALE_X(360), UI_SCALE_Y(360));
    lv_obj_set_pos(soft_limits_panel, UI_SCALE_X(355), UI_SCALE_Y(0));
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

    if (!encoder_timer) {
        encoder_timer = lv_timer_create(encoderTimerCb, 50, nullptr);
    }

    for (size_t i = 0; i < 3; ++i) {
        last_encoder_counts[i] = get_encoder_value(i);
    }
    reset_override_encoder_count();

    syncSoftLimitsUI();
    updateJogDebugInfoUI();
}

void UITabControlJog::setActiveNumericTextarea(lv_obj_t *ta, char axis_hint) {
    if (!ta) {
        return;
    }
    active_numeric_ta = ta;
    if (axis_hint == 'Y' || axis_hint == 'y') {
        active_numeric_axis = 'Y';
    } else if (axis_hint == 'Z' || axis_hint == 'z') {
        active_numeric_axis = 'Z';
    } else {
        active_numeric_axis = 'X';
    }
}

void UITabControlJog::clearActiveNumericTextarea(lv_obj_t *ta) {
    if (!ta || active_numeric_ta == ta) {
        active_numeric_ta = nullptr;
        active_numeric_axis = 'X';
    }
}

bool UITabControlJog::isNumericTextareaCaptureActive() {
    return active_numeric_ta != nullptr;
}

int UITabControlJog::getCurrentXYFeed() {
    if (xy_feedrate_label) {
        const char *current_text = lv_label_get_text(xy_feedrate_label);
        if (current_text && current_text[0] != '\0') {
            return atoi(current_text);
        }
    }
    return xy_current_feed;
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
        const char *feedrate_text = lv_label_get_text(xy_feedrate_label);
        char buf[32];
        snprintf(buf, sizeof(buf), "X:%.2f Y:%.2f\nF:%s", x_current_step, y_current_step, feedrate_text);
        lv_label_set_text(xy_step_display_label, buf);
    }
}

// Update the Z step display between up/down buttons
void UITabControlJog::update_z_step_display() {
    if (z_step_display_label != nullptr && z_feedrate_label != nullptr) {
        const char *feedrate_text = lv_label_get_text(z_feedrate_label);
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
    showSoftLimitsKeyboard(ta);
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
    clearActiveNumericTextarea(soft_limits_active_ta);
    soft_limits_active_ta = nullptr;
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
        const char *current_text = lv_label_get_text(xy_feedrate_label);
        int current_value = atoi(current_text);
        int new_value = current_value + adjustment;
        
        // Clamp to reasonable range (100-10000 mm/min)
        if (new_value < 100) new_value = 100;
        if (new_value > 10000) new_value = 10000;
        
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", new_value);
        lv_label_set_text(xy_feedrate_label, buf);
        xy_current_feed = new_value;
        
        // Update the center display to show new feedrate
        update_xy_step_display();
        
        Serial.printf("XY Feedrate adjusted by %d to: %d mm/min\n", adjustment, new_value);
    }
}

// Z Feedrate adjustment button event handler
void UITabControlJog::z_feedrate_adj_event_cb(lv_event_t *e) {
    int adjustment = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (z_feedrate_label != nullptr) {
        const char *current_text = lv_label_get_text(z_feedrate_label);
        int current_value = atoi(current_text);
        int new_value = current_value + adjustment;
        
        // Clamp to reasonable range (50-5000 mm/min)
        if (new_value < 50) new_value = 50;
        if (new_value > 5000) new_value = 5000;
        
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", new_value);
        lv_label_set_text(z_feedrate_label, buf);
        z_current_feed = new_value;
        
        // Update the center display to show new feedrate
        update_z_step_display();
        
        Serial.printf("Z Feedrate adjusted by %d to: %d mm/min\n", adjustment, new_value);
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
    const char *feedrate_text = lv_label_get_text(xy_feedrate_label);
    int feedrate = atoi(feedrate_text);
    
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

    CommManager::sendJogRelative(x_move, y_move, 0.0f, static_cast<float>(feedrate));
}

// Z Jog button event handler
void UITabControlJog::z_jog_button_event_cb(lv_event_t *e) {
    if (!CommManager::isConnected()) {
        Serial.println("[Jog] Not connected to FluidNC");
        return;
    }
    
    int direction = (int)(intptr_t)lv_event_get_user_data(e);  // +1 for up, -1 for down
    
    // Get current feedrate
    const char *feedrate_text = lv_label_get_text(z_feedrate_label);
    int feedrate = atoi(feedrate_text);
    
    // Calculate Z movement
    float z_move = z_current_step * direction;

    CommManager::sendJogRelative(0.0f, 0.0f, z_move, static_cast<float>(feedrate));
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

    if (active_numeric_ta) {
        if (!lv_obj_is_valid(active_numeric_ta)) {
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
                const float step = (active_numeric_axis == 'Y') ? y_current_step :
                                   (active_numeric_axis == 'Z') ? z_current_step :
                                                                  x_current_step;
                const char *text = lv_textarea_get_text(active_numeric_ta);
                double value = (text && text[0] != '\0') ? strtod(text, nullptr) : 0.0;
                value += static_cast<double>(delta) * static_cast<double>(step);

                char buf[32];
                snprintf(buf, sizeof(buf), "%.4f", value);
                trim_float_string(buf);
                lv_textarea_set_text(active_numeric_ta, buf);
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

    const char *xy_feedrate_text = lv_label_get_text(xy_feedrate_label);
    const char *z_feedrate_text = lv_label_get_text(z_feedrate_label);
    int xy_feedrate = atoi(xy_feedrate_text);
    int z_feedrate = atoi(z_feedrate_text);

    struct AxisCmd {
        char axis;
        float step;
        int feedrate;
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
