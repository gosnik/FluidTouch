#include "ui/tabs/control/ui_tab_control_rotary_table.h"
#include "ui/ui_theme.h"
#include "core/comm_manager.h"
#include "core/encoder.h"
#include "core/power_manager.h"
#include "config.h"
#include <cmath>
#include <cstdlib>

lv_obj_t *UITabControlRotaryTable::parent_tab = nullptr;
lv_obj_t *UITabControlRotaryTable::keyboard = nullptr;
lv_obj_t *UITabControlRotaryTable::ta_center_x = nullptr;
lv_obj_t *UITabControlRotaryTable::ta_center_y = nullptr;
lv_obj_t *UITabControlRotaryTable::ta_radius = nullptr;
lv_obj_t *UITabControlRotaryTable::ta_arc = nullptr;
lv_obj_t *UITabControlRotaryTable::ta_z = nullptr;
lv_obj_t *UITabControlRotaryTable::ta_feed = nullptr;
lv_obj_t *UITabControlRotaryTable::status_label = nullptr;
lv_obj_t *UITabControlRotaryTable::encoder_switch = nullptr;
lv_obj_t *UITabControlRotaryTable::active_field = nullptr;
lv_timer_t *UITabControlRotaryTable::encoder_timer = nullptr;
int16_t UITabControlRotaryTable::last_encoder_count = 0;
bool UITabControlRotaryTable::encoder_enabled = false;
double UITabControlRotaryTable::start_x = 0.0;
double UITabControlRotaryTable::start_y = 0.0;
double UITabControlRotaryTable::current_x = 0.0;
double UITabControlRotaryTable::current_y = 0.0;

static const lv_coord_t RT_FIELD_H = UI_SCALE_Y(44);
static const lv_coord_t RT_FIELD_W = UI_SCALE_X(180);
static const lv_coord_t RT_LABEL_W = UI_SCALE_X(120);

static size_t get_active_encoder_index(lv_obj_t *field) {
    if (field == UITabControlRotaryTable::ta_center_x) {
        return 0;
    }
    if (field == UITabControlRotaryTable::ta_center_y) {
        return 1;
    }
    if (field == UITabControlRotaryTable::ta_z) {
        return 2;
    }
    return 0;
}

void UITabControlRotaryTable::create(lv_obj_t *tab) {
    parent_tab = tab;
    lv_obj_set_style_pad_all(tab, UI_SCALE_X(10), 0);
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(tab);
    lv_label_set_text(title, "ROTARY TABLE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(title, UI_SCALE_X(10), UI_SCALE_Y(5));

    lv_coord_t start_y = UI_SCALE_Y(50);
    lv_coord_t row_gap = UI_SCALE_Y(12);

    auto make_field = [&](const char *label, lv_coord_t y, lv_obj_t **ta_out) {
        lv_obj_t *lbl = lv_label_create(tab);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_pos(lbl, UI_SCALE_X(10), y + UI_SCALE_Y(8));
        lv_obj_set_width(lbl, RT_LABEL_W);

        lv_obj_t *ta = lv_textarea_create(tab);
        lv_obj_set_size(ta, RT_FIELD_W, RT_FIELD_H);
        lv_obj_set_pos(ta, UI_SCALE_X(140), y);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_accepted_chars(ta, "0123456789.-");
        lv_obj_set_style_text_font(ta, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
        *ta_out = ta;
    };

    make_field("Center X:", start_y, &ta_center_x);
    make_field("Center Y:", start_y + (RT_FIELD_H + row_gap), &ta_center_y);
    make_field("Radius:", start_y + 2 * (RT_FIELD_H + row_gap), &ta_radius);
    make_field("Arc Len:", start_y + 3 * (RT_FIELD_H + row_gap), &ta_arc);
    make_field("Z Delta:", start_y + 4 * (RT_FIELD_H + row_gap), &ta_z);

    lv_textarea_set_text(ta_center_x, "0");
    lv_textarea_set_text(ta_center_y, "0");
    lv_textarea_set_text(ta_radius, "0");
    lv_textarea_set_text(ta_arc, "0");
    lv_textarea_set_text(ta_z, "0");

    lv_obj_t *feed_lbl = lv_label_create(tab);
    lv_label_set_text(feed_lbl, "Feed:");
    lv_obj_set_style_text_font(feed_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(feed_lbl, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(feed_lbl, UI_SCALE_X(360), start_y + UI_SCALE_Y(8));

    ta_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_feed, RT_FIELD_W, RT_FIELD_H);
    lv_obj_set_pos(ta_feed, UI_SCALE_X(430), start_y);
    lv_textarea_set_one_line(ta_feed, true);
    lv_textarea_set_accepted_chars(ta_feed, "0123456789");
    lv_textarea_set_text(ta_feed, "1000");
    lv_obj_set_style_text_font(ta_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_feed, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);

    lv_obj_t *encoder_lbl = lv_label_create(tab);
    lv_label_set_text(encoder_lbl, "Encoder:");
    lv_obj_set_style_text_font(encoder_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(encoder_lbl, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(encoder_lbl, UI_SCALE_X(360), start_y + UI_SCALE_Y(70));

    encoder_switch = lv_switch_create(tab);
    lv_obj_set_pos(encoder_switch, UI_SCALE_X(450), start_y + UI_SCALE_Y(66));
    lv_obj_add_event_cb(encoder_switch, onEncoderToggle, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *btn_set = lv_button_create(tab);
    lv_obj_set_size(btn_set, UI_SCALE_X(200), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_set, UI_SCALE_X(360), start_y + UI_SCALE_Y(130));
    lv_obj_set_style_bg_color(btn_set, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(btn_set, onSetStartPressed, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_set_lbl = lv_label_create(btn_set);
    lv_label_set_text(btn_set_lbl, "Set Start = Current");
    lv_obj_set_style_text_font(btn_set_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(btn_set_lbl);

    lv_obj_t *btn_radius = lv_button_create(tab);
    lv_obj_set_size(btn_radius, UI_SCALE_X(200), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_radius, UI_SCALE_X(360), start_y + UI_SCALE_Y(200));
    lv_obj_set_style_bg_color(btn_radius, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_radius, onApplyRadiusPressed, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_radius_lbl = lv_label_create(btn_radius);
    lv_label_set_text(btn_radius_lbl, "Apply Radius");
    lv_obj_set_style_text_font(btn_radius_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(btn_radius_lbl);

    lv_obj_t *btn_arc = lv_button_create(tab);
    lv_obj_set_size(btn_arc, UI_SCALE_X(200), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_arc, UI_SCALE_X(360), start_y + UI_SCALE_Y(270));
    lv_obj_set_style_bg_color(btn_arc, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_arc, onApplyArcPressed, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_arc_lbl = lv_label_create(btn_arc);
    lv_label_set_text(btn_arc_lbl, "Apply Arc");
    lv_obj_set_style_text_font(btn_arc_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(btn_arc_lbl);

    lv_obj_t *btn_z = lv_button_create(tab);
    lv_obj_set_size(btn_z, UI_SCALE_X(200), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_z, UI_SCALE_X(360), start_y + UI_SCALE_Y(340));
    lv_obj_set_style_bg_color(btn_z, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_z, onApplyZPressed, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_z_lbl = lv_label_create(btn_z);
    lv_label_set_text(btn_z_lbl, "Apply Z");
    lv_obj_set_style_text_font(btn_z_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(btn_z_lbl);

    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(status_label, UI_SCALE_X(10), UI_SCALE_Y(400));

    if (!encoder_timer) {
        encoder_timer = lv_timer_create(encoderTimerCb, 50, nullptr);
    }
}

void UITabControlRotaryTable::onSetStartPressed(lv_event_t *e) {
    LV_UNUSED(e);
    const FluidNCStatus &status = CommManager::getStatus();
    start_x = status.wpos_x;
    start_y = status.wpos_y;
    current_x = start_x;
    current_y = start_y;

    setStatus("Start set to current position.", UITheme::STATE_IDLE);
}

void UITabControlRotaryTable::onApplyRadiusPressed(lv_event_t *e) {
    LV_UNUSED(e);
    if (!CommManager::isConnected()) {
        setStatus("Not connected to machine.", UITheme::STATE_ALARM);
        return;
    }

    double cx = 0.0;
    double cy = 0.0;
    double r = 0.0;
    if (!parseDouble(ta_center_x, cx) || !parseDouble(ta_center_y, cy) || !parseDouble(ta_radius, r)) {
        setStatus("Invalid center or radius.", UITheme::STATE_ALARM);
        return;
    }

    findNewPointByRadius(current_x, current_y, cx, cy, r);
    start_x = current_x;
    start_y = current_y;

    int feed = parseFeed(ta_feed);
    const FluidNCStatus &status = CommManager::getStatus();
    const char *units = status.modal_units[0] != '\0' ? status.modal_units : "G21";
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "$J=%sG90 X%.3f Y%.3f F%d\n", units, current_x, current_y, feed);
    CommManager::sendCommand(cmd);
    setStatus("Radius move sent.", UITheme::STATE_IDLE);
}

void UITabControlRotaryTable::onApplyArcPressed(lv_event_t *e) {
    LV_UNUSED(e);
    if (!CommManager::isConnected()) {
        setStatus("Not connected to machine.", UITheme::STATE_ALARM);
        return;
    }

    double cx = 0.0;
    double cy = 0.0;
    double r = 0.0;
    double length = 0.0;
    if (!parseDouble(ta_center_x, cx) || !parseDouble(ta_center_y, cy) ||
        !parseDouble(ta_radius, r) || !parseDouble(ta_arc, length)) {
        setStatus("Invalid center/radius/arc length.", UITheme::STATE_ALARM);
        return;
    }

    bool clockwise = length < 0.0;
    double new_x = start_x;
    double new_y = start_y;
    findArcSecondPoint(new_x, new_y, cx, cy, std::abs(length), clockwise);
    current_x = new_x;
    current_y = new_y;

    int feed = parseFeed(ta_feed);
    const FluidNCStatus &status = CommManager::getStatus();
    const char *units = status.modal_units[0] != '\0' ? status.modal_units : "G21";
    char cmd[160];
    snprintf(cmd, sizeof(cmd), "%sG17G90%s X%.3f Y%.3f R%.3f F%d\n",
             units, clockwise ? "G02" : "G03", current_x, current_y, r, feed);
    CommManager::sendCommand(cmd);
    setStatus("Arc move sent.", UITheme::STATE_IDLE);
}

void UITabControlRotaryTable::onApplyZPressed(lv_event_t *e) {
    LV_UNUSED(e);
    if (!CommManager::isConnected()) {
        setStatus("Not connected to machine.", UITheme::STATE_ALARM);
        return;
    }

    double z_delta = 0.0;
    if (!parseDouble(ta_z, z_delta) || z_delta == 0.0) {
        setStatus("Invalid Z delta.", UITheme::STATE_ALARM);
        return;
    }

    int feed = parseFeed(ta_feed);
    const FluidNCStatus &status = CommManager::getStatus();
    const char *units = status.modal_units[0] != '\0' ? status.modal_units : "G21";
    char cmd[96];
    snprintf(cmd, sizeof(cmd), "$J=%sG91 Z%.3f F%d\n", units, z_delta, feed);
    CommManager::sendCommand(cmd);
    setStatus("Z jog sent.", UITheme::STATE_IDLE);
}

void UITabControlRotaryTable::setStatus(const char *text, lv_color_t color) {
    if (!status_label) {
        return;
    }
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_color(status_label, color, 0);
}

void UITabControlRotaryTable::onTextareaFocused(lv_event_t *e) {
    lv_obj_t *ta = static_cast<lv_obj_t *>(lv_event_get_target(e));
    active_field = ta;
    if (encoder_enabled) {
        last_encoder_count = get_encoder_value(get_active_encoder_index(active_field));
        return;
    }
    showKeyboard(ta);
}

void UITabControlRotaryTable::showKeyboard(lv_obj_t *ta) {
    if (!keyboard) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(keyboard, SCREEN_WIDTH, UI_SCALE_Y(220));
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabControlRotaryTable::hideKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabControlRotaryTable::hideKeyboard(); }, LV_EVENT_CANCEL, nullptr);
        if (parent_tab) {
            lv_obj_add_event_cb(parent_tab, [](lv_event_t *e) { UITabControlRotaryTable::hideKeyboard(); }, LV_EVENT_CLICKED, nullptr);
        }
    }

    if (parent_tab) {
        lv_obj_set_style_pad_bottom(parent_tab, UI_SCALE_Y(240), 0);
        lv_coord_t ta_y = lv_obj_get_y(ta);
        lv_coord_t ta_h = lv_obj_get_height(ta);
        lv_coord_t visible_height = UI_SCALE_Y(200);
        lv_coord_t target_position = visible_height - ta_h - UI_SCALE_Y(10);
        lv_coord_t scroll_y = ta_y - target_position;
        if (scroll_y < 0) scroll_y = 0;
        lv_obj_scroll_to_y(parent_tab, scroll_y, LV_ANIM_ON);
    }

    lv_keyboard_set_textarea(keyboard, ta);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

void UITabControlRotaryTable::hideKeyboard() {
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (parent_tab) {
        lv_obj_set_style_pad_bottom(parent_tab, 0, 0);
        lv_obj_scroll_to_y(parent_tab, 0, LV_ANIM_OFF);
    }
}

void UITabControlRotaryTable::onEncoderToggle(lv_event_t *e) {
    LV_UNUSED(e);
    encoder_enabled = lv_obj_has_state(encoder_switch, LV_STATE_CHECKED);
    if (encoder_enabled) {
        last_encoder_count = get_encoder_value(get_active_encoder_index(active_field));
        hideKeyboard();
    }
}

void UITabControlRotaryTable::encoderTimerCb(lv_timer_t *timer) {
    LV_UNUSED(timer);
    if (!encoder_enabled || !active_field) {
        return;
    }
    size_t encoder_index = get_active_encoder_index(active_field);
    int16_t count = get_encoder_value(encoder_index);
    int16_t delta = count - last_encoder_count;
    if (delta == 0) {
        return;
    }
    last_encoder_count = count;
    PowerManager::onUserActivity();

    const char *text = lv_textarea_get_text(active_field);
    double value = text && text[0] != '\0' ? strtod(text, nullptr) : 0.0;
    bool is_feed = (active_field == ta_feed);
    double step = is_feed ? 10.0 : 0.1;
    value += static_cast<double>(delta) * step;

    char buf[32];
    if (is_feed) {
        if (value < 1.0) value = 1.0;
        snprintf(buf, sizeof(buf), "%.0f", value);
    } else {
        snprintf(buf, sizeof(buf), "%.3f", value);
    }
    lv_textarea_set_text(active_field, buf);
}

bool UITabControlRotaryTable::parseDouble(lv_obj_t *ta, double &out) {
    const char *text = lv_textarea_get_text(ta);
    if (!text || text[0] == '\0') {
        return false;
    }
    char *end = nullptr;
    out = strtod(text, &end);
    return end && *end == '\0';
}

int UITabControlRotaryTable::parseFeed(lv_obj_t *ta) {
    const char *text = lv_textarea_get_text(ta);
    int feed = atoi(text);
    if (feed <= 0) {
        feed = 1000;
    }
    return feed;
}

void UITabControlRotaryTable::findArcSecondPoint(double &xp, double &yp, double xc, double yc, double length, bool clockwise) {
    double r = std::sqrt(std::pow(xp - xc, 2) + std::pow(yp - yc, 2));
    if (r <= 0.0) {
        return;
    }
    double angle = std::atan2(yp - yc, xp - xc);
    angle = clockwise ? (angle - length / r) : (angle + length / r);
    xp = xc + r * std::cos(angle);
    yp = yc + r * std::sin(angle);
}

void UITabControlRotaryTable::findNewPointByRadius(double &xp, double &yp, double xc, double yc, double new_radius) {
    double angle = std::atan2(yp - yc, xp - xc);
    xp = xc + new_radius * std::cos(angle);
    yp = yc + new_radius * std::sin(angle);
}
