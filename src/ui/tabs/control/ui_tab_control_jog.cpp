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
#include <cstdlib>

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
lv_obj_t *UITabControlJog::soft_limits_switch_x = nullptr;
lv_obj_t *UITabControlJog::soft_limits_switch_y = nullptr;
lv_obj_t *UITabControlJog::soft_limits_switch_z = nullptr;
lv_obj_t *UITabControlJog::soft_limits_x_min_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_x_max_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_y_min_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_y_max_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_z_min_ta = nullptr;
lv_obj_t *UITabControlJog::soft_limits_z_max_ta = nullptr;
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
float UITabControlJog::soft_limit_x_min = 0.0f;
float UITabControlJog::soft_limit_x_max = 0.0f;
float UITabControlJog::soft_limit_y_min = 0.0f;
float UITabControlJog::soft_limit_y_max = 0.0f;
float UITabControlJog::soft_limit_z_min = 0.0f;
float UITabControlJog::soft_limit_z_max = 0.0f;

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
    lv_obj_set_pos(encoder_bind_container, UI_SCALE_X(265), UI_SCALE_Y(9));
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
        lv_obj_set_pos(btn, bind_btn_w, bind_row_y + i * (bind_btn_h + bind_btn_gap));
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

    // Soft limits button (opens modal editor)
    // soft_limits_button = lv_button_create(tab);
    // lv_obj_set_size(soft_limits_button, UI_SCALE_X(140), UI_SCALE_Y(40));
    // lv_obj_set_pos(soft_limits_button, UI_SCALE_X(360), UI_SCALE_Y(210));
    // lv_obj_add_event_cb(soft_limits_button, soft_limits_button_event_cb, LV_EVENT_CLICKED, nullptr);
    // lv_obj_t *lbl_soft_limits = lv_label_create(soft_limits_button);
    // lv_label_set_text(lbl_soft_limits, "Soft Limits");
    // lv_obj_set_style_text_font(lbl_soft_limits, &lv_font_montserrat_16, 0);
    // lv_obj_center(lbl_soft_limits);
    
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
    lv_obj_set_pos(soft_limits_panel, UI_SCALE_X(360), UI_SCALE_Y(0));
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
    const lv_coord_t switch_x = UI_SCALE_X(25);
    const lv_coord_t min_label_x = UI_SCALE_X(75);
    const lv_coord_t min_field_x = UI_SCALE_X(110);
    const lv_coord_t max_label_x = UI_SCALE_X(185);
    const lv_coord_t max_field_x = UI_SCALE_X(220);
    const lv_coord_t field_w = UI_SCALE_X(70);
    const lv_coord_t field_h = UI_SCALE_Y(40);

    auto make_axis_row = [&](const char *axis,
                             lv_coord_t y,
                             lv_obj_t **sw_out,
                             lv_obj_t **min_out,
                             lv_obj_t **max_out) {
        lv_obj_t *lbl_axis = lv_label_create(soft_limits_panel);
        lv_label_set_text(lbl_axis, axis);
        lv_obj_set_style_text_font(lbl_axis, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_axis, UITheme::TEXT_MEDIUM, 0);
        lv_obj_set_pos(lbl_axis, axis_x, y + UI_SCALE_Y(12));

        lv_obj_t *sw = lv_switch_create(soft_limits_panel);
        lv_obj_set_pos(sw, switch_x, y + UI_SCALE_Y(6));
        *sw_out = sw;

        lv_obj_t *lbl_min = lv_label_create(soft_limits_panel);
        lv_label_set_text(lbl_min, "Min:");
        lv_obj_set_style_text_font(lbl_min, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_min, UITheme::TEXT_MEDIUM, 0);
        lv_obj_set_pos(lbl_min, min_label_x, y + UI_SCALE_Y(12));

        lv_obj_t *ta_min = lv_textarea_create(soft_limits_panel);
        lv_obj_set_size(ta_min, field_w, field_h);
        lv_obj_set_pos(ta_min, min_field_x, y);
        lv_textarea_set_one_line(ta_min, true);
        lv_textarea_set_max_length(ta_min, 10);
        lv_textarea_set_accepted_chars(ta_min, "0123456789.-");
        lv_obj_set_style_text_font(ta_min, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta_min, soft_limits_textarea_focused_event_cb, LV_EVENT_FOCUSED, nullptr);
        *min_out = ta_min;

        lv_obj_t *lbl_max = lv_label_create(soft_limits_panel);
        lv_label_set_text(lbl_max, "Max:");
        lv_obj_set_style_text_font(lbl_max, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_max, UITheme::TEXT_MEDIUM, 0);
        lv_obj_set_pos(lbl_max, max_label_x, y + UI_SCALE_Y(12));

        lv_obj_t *ta_max = lv_textarea_create(soft_limits_panel);
        lv_obj_set_size(ta_max, field_w, field_h);
        lv_obj_set_pos(ta_max, max_field_x, y);
        lv_textarea_set_one_line(ta_max, true);
        lv_textarea_set_max_length(ta_max, 10);
        lv_textarea_set_accepted_chars(ta_max, "0123456789.-");
        lv_obj_set_style_text_font(ta_max, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta_max, soft_limits_textarea_focused_event_cb, LV_EVENT_FOCUSED, nullptr);
        *max_out = ta_max;
    };

    make_axis_row("X", row_start_y, &soft_limits_switch_x, &soft_limits_x_min_ta, &soft_limits_x_max_ta);
    make_axis_row("Y", row_start_y + row_gap, &soft_limits_switch_y, &soft_limits_y_min_ta, &soft_limits_y_max_ta);
    make_axis_row("Z", row_start_y + 2 * row_gap, &soft_limits_switch_z, &soft_limits_z_min_ta, &soft_limits_z_max_ta);

    lv_obj_t *btn_save = lv_button_create(soft_limits_panel);
    lv_obj_set_size(btn_save, UI_SCALE_X(150), UI_SCALE_Y(44));
    lv_obj_set_pos(btn_save, UI_SCALE_X(140), UI_SCALE_Y(220));
    lv_obj_set_style_bg_color(btn_save, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(btn_save, soft_limits_save_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_save);

    if (!encoder_timer) {
        encoder_timer = lv_timer_create(encoderTimerCb, 50, nullptr);
    }

    for (size_t i = 0; i < 3; ++i) {
        last_encoder_counts[i] = get_encoder_value(i);
    }
    reset_override_encoder_count();

    syncSoftLimitsUI();
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
    UICommon::setEncoderBindAxis(index, true);
    update_encoder_bind_button_styles();
    reset_override_encoder_count();
}

void UITabControlJog::update_encoder_bind_button_styles() {
    int axis = UICommon::getEncoderBindAxis();
    for (int i = 0; i < 3; ++i) {
        if (!encoder_bind_buttons[i]) {
            continue;
        }
        if (i == axis) {
            lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::ACCENT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::ACCENT_PRIMARY_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
        } else {
            lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::BG_BUTTON, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(encoder_bind_buttons[i], UITheme::BORDER_LIGHT, LV_PART_MAIN | LV_STATE_PRESSED);
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

void UITabControlJog::soft_limits_save_event_cb(lv_event_t *e) {
    LV_UNUSED(e);
    storeSoftLimitsFromUI();
    saveSoftLimitsToConfig();
    CommManager::setJogSoftLimits(soft_limit_x_enabled,
                                  soft_limit_y_enabled,
                                  soft_limit_z_enabled,
                                  soft_limit_x_min,
                                  soft_limit_x_max,
                                  soft_limit_y_min,
                                  soft_limit_y_max,
                                  soft_limit_z_min,
                                  soft_limit_z_max);
    hideSoftLimitsKeyboard();
}

void UITabControlJog::soft_limits_textarea_focused_event_cb(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
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

void UITabControlJog::hideSoftLimitsKeyboard() {
    if (soft_limits_keyboard) {
        lv_obj_add_flag(soft_limits_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void UITabControlJog::loadSoftLimitsFromConfig() {
    MachineConfig config;
    if (MachineConfigManager::getSelectedMachine(config)) {
        soft_limit_x_enabled = config.soft_limit_x_enabled;
        soft_limit_y_enabled = config.soft_limit_y_enabled;
        soft_limit_z_enabled = config.soft_limit_z_enabled;
        soft_limit_x_min = config.soft_limit_x_min;
        soft_limit_x_max = config.soft_limit_x_max;
        soft_limit_y_min = config.soft_limit_y_min;
        soft_limit_y_max = config.soft_limit_y_max;
        soft_limit_z_min = config.soft_limit_z_min;
        soft_limit_z_max = config.soft_limit_z_max;
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

void UITabControlJog::syncSoftLimitsUI() {
    if (soft_limits_switch_x) {
        if (soft_limit_x_enabled) lv_obj_add_state(soft_limits_switch_x, LV_STATE_CHECKED);
        else lv_obj_clear_state(soft_limits_switch_x, LV_STATE_CHECKED);
    }
    if (soft_limits_switch_y) {
        if (soft_limit_y_enabled) lv_obj_add_state(soft_limits_switch_y, LV_STATE_CHECKED);
        else lv_obj_clear_state(soft_limits_switch_y, LV_STATE_CHECKED);
    }
    if (soft_limits_switch_z) {
        if (soft_limit_z_enabled) lv_obj_add_state(soft_limits_switch_z, LV_STATE_CHECKED);
        else lv_obj_clear_state(soft_limits_switch_z, LV_STATE_CHECKED);
    }

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
    if (soft_limits_switch_x) soft_limit_x_enabled = lv_obj_has_state(soft_limits_switch_x, LV_STATE_CHECKED);
    if (soft_limits_switch_y) soft_limit_y_enabled = lv_obj_has_state(soft_limits_switch_y, LV_STATE_CHECKED);
    if (soft_limits_switch_z) soft_limit_z_enabled = lv_obj_has_state(soft_limits_switch_z, LV_STATE_CHECKED);

    if (soft_limits_x_min_ta) soft_limit_x_min = strtof(lv_textarea_get_text(soft_limits_x_min_ta), nullptr);
    if (soft_limits_x_max_ta) soft_limit_x_max = strtof(lv_textarea_get_text(soft_limits_x_max_ta), nullptr);
    if (soft_limits_y_min_ta) soft_limit_y_min = strtof(lv_textarea_get_text(soft_limits_y_min_ta), nullptr);
    if (soft_limits_y_max_ta) soft_limit_y_max = strtof(lv_textarea_get_text(soft_limits_y_max_ta), nullptr);
    if (soft_limits_z_min_ta) soft_limit_z_min = strtof(lv_textarea_get_text(soft_limits_z_min_ta), nullptr);
    if (soft_limits_z_max_ta) soft_limit_z_max = strtof(lv_textarea_get_text(soft_limits_z_max_ta), nullptr);
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
        for (size_t i = 0; i < 3; ++i) {
            last_encoder_counts[i] = get_encoder_value(i);
        }
        if (UICommon::isEncoderBindVisible()) {
            reset_override_encoder_count();
        }
        return;
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
                const AxisCmd &axis = axes[UICommon::getEncoderBindAxis()];
                float move = axis.step * static_cast<float>(delta);
                CommManager::sendJogRelativeAxis(axis.axis, move, static_cast<float>(axis.feedrate));
                PowerManager::onUserActivity();
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
