#include "ui/tabs/control/ui_tab_control_jog.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_theme.h"
#include "core/comm_manager.h"
#include "core/encoder.h"
#include "core/power_manager.h"
#include "config.h"
#include <Arduino.h>

// Static member initialization
lv_obj_t *UITabControlJog::parent_tab = nullptr;
lv_obj_t *UITabControlJog::xy_step_display_label = nullptr;
lv_obj_t *UITabControlJog::z_step_display_label = nullptr;
lv_obj_t *UITabControlJog::x_step_buttons[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t *UITabControlJog::y_step_buttons[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t *UITabControlJog::z_step_buttons[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t *UITabControlJog::xy_feedrate_label = nullptr;
lv_obj_t *UITabControlJog::z_feedrate_label = nullptr;
lv_timer_t *UITabControlJog::encoder_timer = nullptr;
int16_t UITabControlJog::last_encoder_counts[3] = {0, 0, 0};
float UITabControlJog::x_current_step = 10.0f;      // Will be loaded from settings
float UITabControlJog::y_current_step = 10.0f;      // Will be loaded from settings
float UITabControlJog::z_current_step = 1.0f;       // Will be loaded from settings
int UITabControlJog::x_current_step_index = 2;      // Will be recalculated
int UITabControlJog::y_current_step_index = 2;      // Will be recalculated
int UITabControlJog::z_current_step_index = 1;      // Will be recalculated
int UITabControlJog::xy_current_feed = 3000;        // Will be loaded from settings
int UITabControlJog::z_current_feed = 1000;         // Will be loaded from settings

void UITabControlJog::create(lv_obj_t *tab) {
    parent_tab = tab;
    // Load default values from settings
    UITabSettingsJog::loadPreferences();
    x_current_step = UITabSettingsJog::getDefaultXYStep();
    y_current_step = UITabSettingsJog::getDefaultXYStep();
    z_current_step = UITabSettingsJog::getDefaultZStep();
    xy_current_feed = UITabSettingsJog::getDefaultXYFeed();
    z_current_feed = UITabSettingsJog::getDefaultZFeed();
    
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
    
    // XY header
    lv_obj_t *xy_jog_header = lv_label_create(tab);
    lv_label_set_text(xy_jog_header, "XY STEP");
    lv_obj_set_style_text_font(xy_jog_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(xy_jog_header, UITheme::AXIS_XY, 0);
    lv_obj_set_pos(xy_jog_header, UI_SCALE_X(120), UI_SCALE_Y(5));
    
    // X Step size selection
    lv_obj_t *x_step_label = lv_label_create(tab);
    lv_label_set_text(x_step_label, "X Step");
    lv_obj_set_style_text_font(x_step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(x_step_label, UI_SCALE_X(5), UI_SCALE_Y(9));
    
    // X Step buttons
    for (int i = 0; i < UITheme::XY_STEP_COUNT; i++) {
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, UI_SCALE_X(40), UI_SCALE_Y(40));
        lv_obj_set_pos(btn_step, UI_SCALE_X(8), UI_SCALE_Y(30) + i * UI_SCALE_Y(42));
        lv_obj_add_event_cb(btn_step, x_step_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        x_step_buttons[i] = btn_step;
        
        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, UITheme::XY_STEP_LABELS[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    update_x_step_button_styles();

    // Y Step size selection
    lv_obj_t *y_step_label = lv_label_create(tab);
    lv_label_set_text(y_step_label, "Y Step");
    lv_obj_set_style_text_font(y_step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(y_step_label, UI_SCALE_X(58), UI_SCALE_Y(9));

    for (int i = 0; i < UITheme::XY_STEP_COUNT; i++) {
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, UI_SCALE_X(40), UI_SCALE_Y(40));
        lv_obj_set_pos(btn_step, UI_SCALE_X(58), UI_SCALE_Y(30) + i * UI_SCALE_Y(42));
        lv_obj_add_event_cb(btn_step, y_step_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        y_step_buttons[i] = btn_step;

        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, UITheme::XY_STEP_LABELS[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    update_y_step_button_styles();
    
    // XY Feed rate control
    lv_obj_t *xy_feed_label = lv_label_create(tab);
    lv_label_set_text(xy_feed_label, "XY Feed:");
    lv_obj_set_style_text_font(xy_feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(xy_feed_label, UI_SCALE_X(120), UI_SCALE_Y(250));
    
    // XY Feedrate value (plain text label) - load from settings
    xy_feedrate_label = lv_label_create(tab);
    char xy_feed_buf[16];
    snprintf(xy_feed_buf, sizeof(xy_feed_buf), "%d", xy_current_feed);
    lv_label_set_text(xy_feedrate_label, xy_feed_buf);
    lv_obj_set_style_text_font(xy_feedrate_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(xy_feedrate_label, UI_SCALE_X(190), UI_SCALE_Y(250));
    
    // Now update XY step display (after feedrate label exists)
    update_xy_step_display();
    
    lv_obj_t *xy_feed_unit = lv_label_create(tab);
    lv_label_set_text(xy_feed_unit, "mm/min");
    lv_obj_set_style_text_font(xy_feed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(xy_feed_unit, UI_SCALE_X(240), UI_SCALE_Y(250));
    
    // XY Feedrate adjustment buttons - all on one line: -1000, -100, +100, +1000
    lv_obj_t *btn_xy_minus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_minus1000, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_minus1000, UI_SCALE_X(120), UI_SCALE_Y(270));
    lv_obj_add_event_cb(btn_xy_minus1000, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1000);
    lv_obj_t *lbl_xy_minus1000 = lv_label_create(btn_xy_minus1000);
    lv_label_set_text(lbl_xy_minus1000, "-1000");
    lv_obj_set_style_text_font(lbl_xy_minus1000, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_minus1000);
    
    lv_obj_t *btn_xy_minus100 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_minus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_minus100, UI_SCALE_X(180), UI_SCALE_Y(270));
    lv_obj_add_event_cb(btn_xy_minus100, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-100);
    lv_obj_t *lbl_xy_minus100 = lv_label_create(btn_xy_minus100);
    lv_label_set_text(lbl_xy_minus100, "-100");
    lv_obj_set_style_text_font(lbl_xy_minus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_minus100);
    
    lv_obj_t *btn_xy_plus100 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_plus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_plus100, UI_SCALE_X(240), UI_SCALE_Y(270));
    lv_obj_add_event_cb(btn_xy_plus100, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)100);
    lv_obj_t *lbl_xy_plus100 = lv_label_create(btn_xy_plus100);
    lv_label_set_text(lbl_xy_plus100, "+100");
    lv_obj_set_style_text_font(lbl_xy_plus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_plus100);
    
    lv_obj_t *btn_xy_plus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_plus1000, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_xy_plus1000, UI_SCALE_X(300), UI_SCALE_Y(270));
    lv_obj_add_event_cb(btn_xy_plus1000, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1000);
    lv_obj_t *lbl_xy_plus1000 = lv_label_create(btn_xy_plus1000);
    lv_label_set_text(lbl_xy_plus1000, "+1000");
    lv_obj_set_style_text_font(lbl_xy_plus1000, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_xy_plus1000);
    
    // ========== Z Section (Right side) ==========
    
    // Z Jog header - centered above Z+ button
    lv_obj_t *z_jog_header = lv_label_create(tab);
    lv_label_set_text(z_jog_header, "Z JOG");
    lv_obj_set_style_text_font(z_jog_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(z_jog_header, UITheme::AXIS_Z, 0);
    lv_obj_set_pos(z_jog_header, UI_SCALE_X(467), UI_SCALE_Y(5));  // Centered above Z+ button at x=460
    
    // Z Step size selection - vertical buttons
    lv_obj_t *z_step_label = lv_label_create(tab);
    lv_label_set_text(z_step_label, "Z Step");
    lv_obj_set_style_text_font(z_step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(z_step_label, UI_SCALE_X(395), UI_SCALE_Y(9));
    
    // Z Step size buttons - vertical (largest to smallest)
    for (int i = 0; i < UITheme::Z_STEP_COUNT; i++) {
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, UI_SCALE_X(40), UI_SCALE_Y(40));
        lv_obj_set_pos(btn_step, UI_SCALE_X(395), UI_SCALE_Y(30) + i * UI_SCALE_Y(42));
        lv_obj_add_event_cb(btn_step, z_step_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        z_step_buttons[i] = btn_step;
        
        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, UITheme::Z_STEP_LABELS[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    update_z_step_button_styles();
    
    z_step_display_label = nullptr;
    
    // Z Feed rate control
    lv_obj_t *z_feed_label = lv_label_create(tab);
    lv_label_set_text(z_feed_label, "Z Feed:");
    lv_obj_set_style_text_font(z_feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(z_feed_label, UI_SCALE_X(395), UI_SCALE_Y(250));
    
    // Z Feedrate value (plain text label) - load from settings
    z_feedrate_label = lv_label_create(tab);
    char z_feed_buf[16];
    snprintf(z_feed_buf, sizeof(z_feed_buf), "%d", z_current_feed);
    lv_label_set_text(z_feedrate_label, z_feed_buf);
    lv_obj_set_style_text_font(z_feedrate_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(z_feedrate_label, UI_SCALE_X(460), UI_SCALE_Y(250));
    
    // Now update Z step display (after feedrate label exists)
    update_z_step_display();
    
    lv_obj_t *z_feed_unit = lv_label_create(tab);
    lv_label_set_text(z_feed_unit, "mm/min");
    lv_obj_set_style_text_font(z_feed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(z_feed_unit, UI_SCALE_X(505), UI_SCALE_Y(280));
    
    // Z Feedrate adjustment buttons - all on one line: -1000, -100, +100, +1000
    lv_obj_t *btn_z_minus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_z_minus1000, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_minus1000, UI_SCALE_X(395), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_minus1000, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1000);
    lv_obj_t *lbl_z_minus1000 = lv_label_create(btn_z_minus1000);
    lv_label_set_text(lbl_z_minus1000, "-1000");
    lv_obj_set_style_text_font(lbl_z_minus1000, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_minus1000);
    
    lv_obj_t *btn_z_minus100 = lv_button_create(tab);
    lv_obj_set_size(btn_z_minus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_minus100, UI_SCALE_X(455), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_minus100, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-100);
    lv_obj_t *lbl_z_minus100 = lv_label_create(btn_z_minus100);
    lv_label_set_text(lbl_z_minus100, "-100");
    lv_obj_set_style_text_font(lbl_z_minus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_minus100);
    
    lv_obj_t *btn_z_plus100 = lv_button_create(tab);
    lv_obj_set_size(btn_z_plus100, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_plus100, UI_SCALE_X(515), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_plus100, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)100);
    lv_obj_t *lbl_z_plus100 = lv_label_create(btn_z_plus100);
    lv_label_set_text(lbl_z_plus100, "+100");
    lv_obj_set_style_text_font(lbl_z_plus100, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_plus100);
    
    lv_obj_t *btn_z_plus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_z_plus1000, UI_SCALE_X(55), UI_SCALE_Y(45));
    lv_obj_set_pos(btn_z_plus1000, UI_SCALE_X(575), UI_SCALE_Y(300));
    lv_obj_add_event_cb(btn_z_plus1000, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1000);
    lv_obj_t *lbl_z_plus1000 = lv_label_create(btn_z_plus1000);
    lv_label_set_text(lbl_z_plus1000, "+1000");
    lv_obj_set_style_text_font(lbl_z_plus1000, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_z_plus1000);
    
    // ========== Cancel Jog Button (Upper Right) ==========
    // Create a container for the octagon stop button
    lv_obj_t *btn_cancel = lv_obj_create(tab);
    lv_obj_set_size(btn_cancel, UI_SCALE_X(70), UI_SCALE_Y(70));
    lv_obj_set_pos(btn_cancel, UI_SCALE_X(560), UI_SCALE_Y(110));  // Aligned with middle row (left/right buttons)
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
    
    // Build jog command: $J=G91 X[x] Y[y] F[feedrate]
    char jog_cmd[64];
    if (x_move != 0 && y_move != 0) {
        // Diagonal move
        snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 X%.3f Y%.3f F%d\n", x_move, y_move, feedrate);
    } else if (x_move != 0) {
        // X only
        snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 X%.3f F%d\n", x_move, feedrate);
    } else if (y_move != 0) {
        // Y only
        snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 Y%.3f F%d\n", y_move, feedrate);
    }
    
    Serial.printf("[Jog] XY Jog: %s", jog_cmd);
    CommManager::sendCommand(jog_cmd);
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
    
    // Build jog command: $J=G91 Z[z] F[feedrate]
    char jog_cmd[64];
    snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 Z%.3f F%d\n", z_move, feedrate);
    
    Serial.printf("[Jog] Z Jog: %s", jog_cmd);
    CommManager::sendCommand(jog_cmd);
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

    if (parent_tab && lv_obj_has_flag(parent_tab, LV_OBJ_FLAG_HIDDEN)) {
        for (size_t i = 0; i < 3; ++i) {
            last_encoder_counts[i] = get_encoder_value(i);
        }
        return;
    }

    if (!CommManager::isConnected()) {
        for (size_t i = 0; i < 3; ++i) {
            last_encoder_counts[i] = get_encoder_value(i);
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

    for (size_t i = 0; i < 3; ++i) {
        int16_t count = get_encoder_value(i);
        int16_t delta = count - last_encoder_counts[i];
        last_encoder_counts[i] = count;
        if (delta == 0) {
            continue;
        }

        float move = axes[i].step * static_cast<float>(delta);
        char jog_cmd[64];
        snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 %c%.3f F%d\n",
                 axes[i].axis, move, axes[i].feedrate);
        ESP_LOGI("UI", "[Jog] Encoder %u jog: %s", static_cast<unsigned>(i + 1), jog_cmd);
        CommManager::sendCommand(jog_cmd);
        PowerManager::onUserActivity();
    }
}
