#include "ui/tabs/control/ui_tab_control_joystick.h"
#include "ui/tabs/control/ui_tab_control_jog.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "core/comm_manager.h"
#include "core/encoder.h"
#include "core/power_manager.h"
#include "config.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMt2AngleDeg = 1.4300;  // Nominal Morse taper half-angle
constexpr double kMt3AngleDeg = 1.4330;  // Nominal Morse taper half-angle
constexpr uint32_t kStepButtonCount = 6;
constexpr uint32_t kPresetCount = 7;

struct CrossSlidePreset {
    const char *name;
    double angle_deg;
};

constexpr CrossSlidePreset kPresets[kPresetCount] = {
    {"0 deg", 0.0},
    {"30 deg", 30.0},
    {"45 deg", 45.0},
    {"60 deg", 60.0},
    {"90 deg", 90.0},
    {"MT2", kMt2AngleDeg},
    {"MT3", kMt3AngleDeg},
};

lv_obj_t *parent_tab = nullptr;
lv_obj_t *keyboard = nullptr;
lv_obj_t *status_label = nullptr;
lv_obj_t *angle_label = nullptr;
lv_obj_t *formula_label = nullptr;
lv_obj_t *feed_label = nullptr;
lv_obj_t *residue_label = nullptr;
lv_obj_t *ta_degrees = nullptr;
lv_obj_t *ta_taper = nullptr;
lv_obj_t *ta_length = nullptr;
lv_obj_t *ta_ratio_num = nullptr;
lv_obj_t *ta_ratio_den = nullptr;
lv_obj_t *preset_dropdown = nullptr;
lv_obj_t *encoder_switch = nullptr;
lv_obj_t *active_field = nullptr;
lv_obj_t *step_buttons[kStepButtonCount] = {nullptr};
lv_timer_t *encoder_timer = nullptr;

int16_t last_encoder_count = 0;
bool encoder_enabled = false;
double active_angle_deg = 0.0;
double residual_x = 0.0;
double residual_y = 0.0;
float current_step = 0.01f;
int current_step_index = 5;

lv_coord_t field_h() { return UI_SCALE_Y(44); }
lv_coord_t field_w() { return UI_SCALE_X(110); }
lv_coord_t label_w() { return UI_SCALE_X(130); }

double normalize_angle(double degrees) {
    while (degrees <= -180.0) {
        degrees += 360.0;
    }
    while (degrees > 180.0) {
        degrees -= 360.0;
    }
    return degrees;
}

void set_status(const char *text, lv_color_t color) {
    if (!status_label) {
        return;
    }
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_color(status_label, color, 0);
}

void update_step_button_styles() {
    for (uint32_t i = 0; i < kStepButtonCount; ++i) {
        if (!step_buttons[i]) {
            continue;
        }
        bool selected = static_cast<int>(i) == current_step_index;
        lv_obj_set_style_bg_color(step_buttons[i], selected ? UITheme::ACCENT_SECONDARY : UITheme::BG_BUTTON, 0);
        lv_obj_set_style_text_color(step_buttons[i], lv_color_white(), 0);
    }
}

void update_summary_labels() {
    if (angle_label) {
        lv_label_set_text_fmt(angle_label, "Theta: %.4f deg", active_angle_deg);
    }

    const double radians = active_angle_deg * kPi / 180.0;
    const double x_step = static_cast<double>(current_step) * std::cos(radians);
    const double y_step = static_cast<double>(current_step) * std::sin(radians);

    if (formula_label) {
        lv_label_set_text_fmt(formula_label,
                              "Step %.4f -> X %.4f, Y %.4f",
                              current_step,
                              x_step,
                              y_step);
    }

    if (feed_label) {
        lv_label_set_text_fmt(feed_label, "Feed: %.3g mm/min", static_cast<double>(UITabControlJog::getCurrentXYFeed()));
    }

    if (residue_label) {
        lv_label_set_text_fmt(residue_label, "Residue X %.6f  Y %.6f", residual_x, residual_y);
    }
}

void apply_angle(double degrees, const char *message) {
    active_angle_deg = normalize_angle(degrees);

    if (ta_degrees) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4f", active_angle_deg);
        lv_textarea_set_text(ta_degrees, buf);
    }

    update_summary_labels();
    set_status(message, UITheme::STATE_IDLE);
}

bool parse_textarea_double(lv_obj_t *ta, double &value) {
    if (!ta) {
        return false;
    }
    const char *text = lv_textarea_get_text(ta);
    if (!text || text[0] == '\0') {
        return false;
    }
    char *end = nullptr;
    value = strtod(text, &end);
    return end && *end == '\0';
}

double field_encoder_step(lv_obj_t *field) {
    if (field == ta_degrees) {
        return 0.1;
    }
    if (field == ta_length || field == ta_ratio_den) {
        return 0.1;
    }
    return 0.001;
}

char field_axis_hint(lv_obj_t *field) {
    if (field == ta_length || field == ta_ratio_den) {
        return 'Y';
    }
    return 'X';
}

void hide_keyboard() {
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (parent_tab) {
        lv_obj_set_style_pad_bottom(parent_tab, 0, 0);
        lv_obj_scroll_to_y(parent_tab, 0, LV_ANIM_OFF);
    }
}

void show_keyboard(lv_obj_t *ta) {
    if (!keyboard) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(keyboard, SCREEN_WIDTH, UI_SCALE_Y(220));
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *) { hide_keyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *) { hide_keyboard(); }, LV_EVENT_CANCEL, nullptr);
    }

    if (parent_tab) {
        lv_obj_set_style_pad_bottom(parent_tab, UI_SCALE_Y(240), 0);
        lv_coord_t ta_y = lv_obj_get_y(ta);
        lv_coord_t ta_h = lv_obj_get_height(ta);
        lv_coord_t visible_height = UI_SCALE_Y(180);
        lv_coord_t target_position = visible_height - ta_h - UI_SCALE_Y(10);
        lv_coord_t scroll_y = ta_y - target_position;
        if (scroll_y < 0) {
            scroll_y = 0;
        }
        lv_obj_scroll_to_y(parent_tab, scroll_y, LV_ANIM_ON);
    }

    lv_keyboard_set_textarea(keyboard, ta);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

bool send_virtual_move(double axis_delta, const char *origin) {
    if (!CommManager::isConnected()) {
        set_status("Machine not connected.", UITheme::STATE_ALARM);
        return false;
    }

    if (axis_delta == 0.0) {
        return false;
    }

    const double radians = active_angle_deg * kPi / 180.0;
    residual_x += axis_delta * std::cos(radians);
    residual_y += axis_delta * std::sin(radians);

    const float x_move = static_cast<float>(residual_x);
    const float y_move = static_cast<float>(residual_y);
    if (x_move == 0.0f && y_move == 0.0f) {
        update_summary_labels();
        return false;
    }

    if (!CommManager::sendJogRelative(x_move, y_move, 0.0f, static_cast<float>(UITabControlJog::getCurrentXYFeed()))) {
        set_status("Cross Slide move rejected.", UITheme::STATE_ALARM);
        return false;
    }

    residual_x -= static_cast<double>(x_move);
    residual_y -= static_cast<double>(y_move);
    update_summary_labels();
    set_status(origin, UITheme::STATE_IDLE);
    return true;
}

void on_degree_apply(lv_event_t *) {
    PowerManager::onUserActivity();
    double degrees = 0.0;
    if (!parse_textarea_double(ta_degrees, degrees)) {
        set_status("Invalid degree entry.", UITheme::STATE_ALARM);
        return;
    }
    apply_angle(degrees, "Angle set from degree entry.");
}

void on_preset_apply(lv_event_t *) {
    PowerManager::onUserActivity();
    uint16_t selected = lv_dropdown_get_selected(preset_dropdown);
    if (selected >= kPresetCount) {
        set_status("Invalid preset selection.", UITheme::STATE_ALARM);
        return;
    }
    apply_angle(kPresets[selected].angle_deg, "Angle set from preset.");
}

void on_taper_apply(lv_event_t *) {
    PowerManager::onUserActivity();
    double taper = 0.0;
    double length = 0.0;
    if (!parse_textarea_double(ta_taper, taper) || !parse_textarea_double(ta_length, length) || length == 0.0) {
        set_status("Invalid taper/length values.", UITheme::STATE_ALARM);
        return;
    }
    apply_angle(std::atan(taper / length) * 180.0 / kPi, "Angle set from taper per length.");
}

void on_ratio_apply(lv_event_t *) {
    PowerManager::onUserActivity();
    double rise = 0.0;
    double run = 0.0;
    if (!parse_textarea_double(ta_ratio_num, rise) || !parse_textarea_double(ta_ratio_den, run) || run == 0.0) {
        set_status("Invalid taper ratio values.", UITheme::STATE_ALARM);
        return;
    }
    apply_angle(std::atan(rise / run) * 180.0 / kPi, "Angle set from taper ratio.");
}

void on_move_button(lv_event_t *e) {
    PowerManager::onUserActivity();
    const intptr_t sign = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    const double axis_delta = (sign >= 0 ? 1.0 : -1.0) * static_cast<double>(current_step);
    send_virtual_move(axis_delta, sign >= 0 ? "Cross Slide + move sent." : "Cross Slide - move sent.");
}

void on_step_button(lv_event_t *e) {
    PowerManager::onUserActivity();
    const intptr_t index = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    if (index < 0 || index >= static_cast<intptr_t>(kStepButtonCount)) {
        return;
    }
    current_step_index = static_cast<int>(index);
    current_step = UITheme::XY_STEP_VALUES[current_step_index];
    update_step_button_styles();
    update_summary_labels();
    set_status("Cross Slide step updated.", UITheme::STATE_IDLE);
}

void on_textarea_focused(lv_event_t *e) {
    active_field = static_cast<lv_obj_t *>(lv_event_get_target(e));
    UITabControlJog::setActiveNumericTextarea(active_field, field_axis_hint(active_field));
    UICommon::registerKeyboardTarget(active_field, show_keyboard, hide_keyboard);
    if (encoder_enabled) {
        last_encoder_count = get_encoder_value(0);
        return;
    }
    if (UICommon::isOnScreenKeyboardEnabled()) {
        show_keyboard(active_field);
    } else {
        hide_keyboard();
    }
}

void on_textarea_defocused(lv_event_t *e) {
    lv_obj_t *ta = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (active_field == ta) {
        active_field = nullptr;
    }
    UITabControlJog::clearActiveNumericTextarea(ta);
    UICommon::clearKeyboardTarget(ta);
}

void on_encoder_toggle(lv_event_t *) {
    PowerManager::onUserActivity();
    encoder_enabled = lv_obj_has_state(encoder_switch, LV_STATE_CHECKED);
    if (encoder_enabled) {
        hide_keyboard();
        last_encoder_count = get_encoder_value(0);
        set_status("Encoder controls Cross Slide.", UITheme::STATE_IDLE);
        return;
    }
    set_status("Encoder disabled for Cross Slide.", UITheme::TEXT_LIGHT);
}

void encoder_timer_cb(lv_timer_t *) {
    if (UITabControlJog::isNumericTextareaCaptureActive() && !active_field) {
        return;
    }
    if (!encoder_enabled || !parent_tab || lv_obj_has_flag(parent_tab, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    int16_t count = get_encoder_value(0);
    int16_t delta = count - last_encoder_count;
    if (delta == 0) {
        return;
    }
    last_encoder_count = count;
    PowerManager::onUserActivity();

    if (active_field) {
        const char *text = lv_textarea_get_text(active_field);
        double value = (text && text[0] != '\0') ? strtod(text, nullptr) : 0.0;
        value += static_cast<double>(delta) * field_encoder_step(active_field);
        char buf[32];
        if (active_field == ta_degrees) {
            snprintf(buf, sizeof(buf), "%.4f", value);
        } else {
            snprintf(buf, sizeof(buf), "%.3f", value);
        }
        lv_textarea_set_text(active_field, buf);
        return;
    }

    const double axis_delta = static_cast<double>(delta) * static_cast<double>(current_step);
    send_virtual_move(axis_delta, "Cross Slide encoder move sent.");
}

lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t width = 0) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(label, x, y);
    if (width > 0) {
        lv_obj_set_width(label, width);
    }
    return label;
}

lv_obj_t *make_textarea(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *initial, const char *accepted) {
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, field_w(), field_h());
    lv_obj_set_pos(ta, x, y);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_accepted_chars(ta, accepted);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_18, 0);
    lv_textarea_set_text(ta, initial);
    lv_obj_add_event_cb(ta, on_textarea_focused, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(ta, on_textarea_defocused, LV_EVENT_DEFOCUSED, nullptr);
    return ta;
}

lv_obj_t *make_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, const char *text, lv_event_cb_t cb, void *user_data, lv_color_t color) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, UI_SCALE_Y(44));
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
    return btn;
}

}  // namespace

void UITabControlJoystick::create(lv_obj_t *tab) {
    parent_tab = tab;
    active_field = nullptr;
    residual_x = 0.0;
    residual_y = 0.0;

    current_step = UITabSettingsJog::getDefaultXYStep();
    current_step_index = static_cast<int>(kStepButtonCount - 1);
    for (uint32_t i = 0; i < kStepButtonCount; ++i) {
        if (std::fabs(UITheme::XY_STEP_VALUES[i] - current_step) < 0.0001f) {
            current_step_index = static_cast<int>(i);
            break;
        }
    }

    lv_obj_set_style_pad_all(tab, UI_SCALE_X(10), 0);
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(tab);
    lv_label_set_text(title, "CROSS SLIDE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(title, UI_SCALE_X(10), UI_SCALE_Y(5));

    auto make_panel = [&](lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) -> lv_obj_t * {
        lv_obj_t *panel = lv_obj_create(tab);
        lv_obj_set_size(panel, w, h);
        lv_obj_set_pos(panel, x, y);
        lv_obj_set_style_bg_color(panel, UITheme::BG_DARKER, 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_border_color(panel, UITheme::BORDER_MEDIUM, 0);
        lv_obj_set_style_radius(panel, UI_SCALE_X(6), 0);
        lv_obj_set_style_pad_all(panel, UI_SCALE_X(10), 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        return panel;
    };

    auto make_section_title = [&](lv_obj_t *parent, const char *text) {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, UITheme::ACCENT_SECONDARY, 0);
        lv_obj_set_pos(label, 0, 0);
    };

    const lv_coord_t summary_x = UI_SCALE_X(10);
    const lv_coord_t summary_y = UI_SCALE_Y(30);
    const lv_coord_t summary_w = UI_SCALE_X(770);
    const lv_coord_t summary_h = UI_SCALE_Y(78);
    const lv_coord_t section_y = UI_SCALE_Y(126);
    const lv_coord_t section_h = UI_SCALE_Y(214);
    const lv_coord_t angle_w = UI_SCALE_X(390);
    const lv_coord_t step_w = UI_SCALE_X(150);
    const lv_coord_t move_w = UI_SCALE_X(210);
    const lv_coord_t gap = UI_SCALE_X(10);

    lv_obj_t *summary_panel = make_panel(summary_x, summary_y, summary_w, summary_h);
    lv_obj_t *angle_panel = make_panel(summary_x, section_y, angle_w, section_h);
    lv_obj_t *step_panel = make_panel(summary_x + angle_w + gap, section_y, step_w, section_h);
    lv_obj_t *move_panel = make_panel(summary_x + angle_w + gap + step_w + gap, section_y, move_w, section_h);

    angle_label = lv_label_create(summary_panel);
    lv_obj_set_style_text_font(angle_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(angle_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(angle_label, 0, 0);

    formula_label = lv_label_create(summary_panel);
    lv_obj_set_style_text_font(formula_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(formula_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(formula_label, 0, UI_SCALE_Y(26));
    lv_obj_set_width(formula_label, UI_SCALE_X(520));

    feed_label = lv_label_create(summary_panel);
    lv_obj_set_style_text_font(feed_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(feed_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(feed_label, 0, UI_SCALE_Y(48));

    residue_label = lv_label_create(summary_panel);
    lv_obj_set_style_text_font(residue_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(residue_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(residue_label, UI_SCALE_X(540), 0);
    lv_obj_set_width(residue_label, UI_SCALE_X(210));

    make_section_title(angle_panel, "ANGLE INPUT");
    make_section_title(step_panel, "STEP");
    make_section_title(move_panel, "MOVE");

    const lv_coord_t left_col_x = 0;
    const lv_coord_t right_col_x = UI_SCALE_X(190);
    const lv_coord_t top_label_y = UI_SCALE_Y(28);
    const lv_coord_t top_field_y = UI_SCALE_Y(52);
    const lv_coord_t bottom_label_y = UI_SCALE_Y(104);
    const lv_coord_t bottom_field_y = UI_SCALE_Y(128);

    make_label(angle_panel, "Degrees", left_col_x, top_label_y, UI_SCALE_X(90));
    ta_degrees = make_textarea(angle_panel, left_col_x, top_field_y, "0.0000", "0123456789.-");
    lv_obj_set_size(ta_degrees, UI_SCALE_X(108), field_h());
    UITabControlJog::registerNavigableNumericField(tab, ta_degrees, 'X');
    make_button(angle_panel, UI_SCALE_X(116), top_field_y, UI_SCALE_X(64), "Set", on_degree_apply, nullptr, UITheme::BTN_PLAY);

    make_label(angle_panel, "Preset", left_col_x, bottom_label_y, UI_SCALE_X(90));
    preset_dropdown = lv_dropdown_create(angle_panel);
    lv_obj_set_size(preset_dropdown, UI_SCALE_X(108), field_h());
    lv_obj_set_pos(preset_dropdown, left_col_x, bottom_field_y);
    lv_obj_set_style_text_font(preset_dropdown, &lv_font_montserrat_18, 0);
    lv_dropdown_set_options(preset_dropdown, "0 deg\n30 deg\n45 deg\n60 deg\n90 deg\nMT2\nMT3");
    make_button(angle_panel, UI_SCALE_X(116), bottom_field_y, UI_SCALE_X(64), "Load", on_preset_apply, nullptr, UITheme::BG_BUTTON);

    make_label(angle_panel, "Taper / Length", right_col_x, top_label_y, UI_SCALE_X(140));
    ta_taper = make_textarea(angle_panel, right_col_x, top_field_y, "0.000", "0123456789.-");
    lv_obj_set_size(ta_taper, UI_SCALE_X(56), field_h());
    UITabControlJog::registerNavigableNumericField(tab, ta_taper, 'X');
    ta_length = make_textarea(angle_panel, right_col_x + UI_SCALE_X(64), top_field_y, "1.000", "0123456789.-");
    lv_obj_set_size(ta_length, UI_SCALE_X(56), field_h());
    UITabControlJog::registerNavigableNumericField(tab, ta_length, 'Y');
    make_button(angle_panel, right_col_x + UI_SCALE_X(128), top_field_y, UI_SCALE_X(52), "Set", on_taper_apply, nullptr, UITheme::BG_BUTTON);

    make_label(angle_panel, "Rise / Run", right_col_x, bottom_label_y, UI_SCALE_X(140));
    ta_ratio_num = make_textarea(angle_panel, right_col_x, bottom_field_y, "1.000", "0123456789.-");
    lv_obj_set_size(ta_ratio_num, UI_SCALE_X(56), field_h());
    UITabControlJog::registerNavigableNumericField(tab, ta_ratio_num, 'X');
    ta_ratio_den = make_textarea(angle_panel, right_col_x + UI_SCALE_X(64), bottom_field_y, "1.000", "0123456789.-");
    lv_obj_set_size(ta_ratio_den, UI_SCALE_X(56), field_h());
    UITabControlJog::registerNavigableNumericField(tab, ta_ratio_den, 'Y');
    make_button(angle_panel, right_col_x + UI_SCALE_X(128), bottom_field_y, UI_SCALE_X(52), "Set", on_ratio_apply, nullptr, UITheme::BG_BUTTON);

    const lv_coord_t step_button_w = UI_SCALE_X(68);
    const lv_coord_t step_gap = UI_SCALE_X(8);
    const lv_coord_t step_start_x = 0;
    const lv_coord_t step_start_y = UI_SCALE_Y(34);
    for (uint32_t i = 0; i < kStepButtonCount; ++i) {
        const lv_coord_t row = static_cast<lv_coord_t>(i / 2);
        const lv_coord_t col = static_cast<lv_coord_t>(i % 2);
        const lv_coord_t x = step_start_x + col * (step_button_w + step_gap);
        const lv_coord_t y = step_start_y + row * UI_SCALE_Y(52);
        step_buttons[i] = make_button(step_panel,
                                      x,
                                      y,
                                      step_button_w,
                                      UITheme::XY_STEP_LABELS[i],
                                      on_step_button,
                                      reinterpret_cast<void *>(static_cast<intptr_t>(i)),
                                      UITheme::BG_BUTTON);
    }

    lv_obj_t *step_hint = lv_label_create(step_panel);
    lv_label_set_text(step_hint, "Selected step controls\nbutton and encoder moves.");
    lv_obj_set_style_text_font(step_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(step_hint, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_width(step_hint, UI_SCALE_X(130));
    lv_obj_set_pos(step_hint, 0, UI_SCALE_Y(170));

    make_label(move_panel, "Encoder", 0, UI_SCALE_Y(34), UI_SCALE_X(80));
    encoder_switch = lv_switch_create(move_panel);
    lv_obj_set_pos(encoder_switch, UI_SCALE_X(100), UI_SCALE_Y(30));
    lv_obj_add_event_cb(encoder_switch, on_encoder_toggle, LV_EVENT_VALUE_CHANGED, nullptr);
    UITabControlJog::registerNavigableSwitch(tab, encoder_switch);

    make_button(move_panel, 0, UI_SCALE_Y(88), UI_SCALE_X(180), "Move -", on_move_button, reinterpret_cast<void *>(static_cast<intptr_t>(-1)), UITheme::BG_BUTTON);
    make_button(move_panel, 0, UI_SCALE_Y(142), UI_SCALE_X(180), "Move +", on_move_button, reinterpret_cast<void *>(static_cast<intptr_t>(1)), UITheme::BTN_PLAY);

    lv_obj_t *hint = lv_label_create(move_panel);
    lv_label_set_text(hint,
                      "Moves are emitted as coordinated X/Y jogs.\n"
                      "Encoder follows theta and preserves residue.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_width(hint, UI_SCALE_X(180));
    lv_obj_set_pos(hint, 0, UI_SCALE_Y(144));

    status_label = lv_label_create(tab);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_bg_color(status_label, UITheme::BG_DARKER, 0);
    lv_obj_set_style_bg_opa(status_label, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_label, 1, 0);
    lv_obj_set_style_border_color(status_label, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_radius(status_label, UI_SCALE_X(6), 0);
    lv_obj_set_style_pad_all(status_label, UI_SCALE_X(8), 0);
    lv_obj_set_width(status_label, UI_SCALE_X(760));
    lv_obj_set_pos(status_label, UI_SCALE_X(10), UI_SCALE_Y(336));

    update_step_button_styles();
    apply_angle(active_angle_deg, "Cross Slide ready.");

    if (!encoder_timer) {
        encoder_timer = lv_timer_create(encoder_timer_cb, 50, nullptr);
    }
}
