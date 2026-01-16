#include "ui/tabs/control/ui_tab_control_power_feed.h"
#include "ui/ui_theme.h"
#include "core/comm_manager.h"
#include "core/encoder.h"
#include "core/power_manager.h"
#include "config.h"
#include <cstdlib>

lv_obj_t *UITabControlPowerFeed::parent_tab = nullptr;
lv_obj_t *UITabControlPowerFeed::keyboard = nullptr;
lv_obj_t *UITabControlPowerFeed::ta_x = nullptr;
lv_obj_t *UITabControlPowerFeed::ta_y = nullptr;
lv_obj_t *UITabControlPowerFeed::ta_z = nullptr;
lv_obj_t *UITabControlPowerFeed::ta_feed = nullptr;
lv_obj_t *UITabControlPowerFeed::mode_switch = nullptr;
lv_obj_t *UITabControlPowerFeed::status_label = nullptr;
lv_obj_t *UITabControlPowerFeed::encoder_switch = nullptr;
lv_obj_t *UITabControlPowerFeed::active_field = nullptr;
lv_timer_t *UITabControlPowerFeed::encoder_timer = nullptr;
int16_t UITabControlPowerFeed::last_encoder_count = 0;
bool UITabControlPowerFeed::encoder_enabled = false;

static const lv_coord_t PF_FIELD_H = UI_SCALE_Y(44);
static const lv_coord_t PF_LABEL_W = UI_SCALE_X(40);
static const lv_coord_t PF_FIELD_W = UI_SCALE_X(180);

static size_t get_active_encoder_index(lv_obj_t *field) {
    if (field == UITabControlPowerFeed::ta_x) {
        return 0;
    }
    if (field == UITabControlPowerFeed::ta_y) {
        return 1;
    }
    if (field == UITabControlPowerFeed::ta_z) {
        return 2;
    }
    return 0;
}

void UITabControlPowerFeed::create(lv_obj_t *tab) {
    parent_tab = tab;
    lv_obj_set_style_pad_all(tab, UI_SCALE_X(10), 0);
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(tab);
    lv_label_set_text(title, "POWER FEED");
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
        lv_obj_set_width(lbl, PF_LABEL_W);

        lv_obj_t *ta = lv_textarea_create(tab);
        lv_obj_set_size(ta, PF_FIELD_W, PF_FIELD_H);
        lv_obj_set_pos(ta, UI_SCALE_X(60), y);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_accepted_chars(ta, "0123456789.-");
        lv_obj_set_style_text_font(ta, &lv_font_montserrat_18, 0);
        lv_obj_add_event_cb(ta, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);
        *ta_out = ta;
    };

    make_field("X:", start_y, &ta_x);
    make_field("Y:", start_y + (PF_FIELD_H + row_gap), &ta_y);
    make_field("Z:", start_y + 2 * (PF_FIELD_H + row_gap), &ta_z);

    lv_coord_t right_x = UI_SCALE_X(320);
    lv_obj_t *feed_lbl = lv_label_create(tab);
    lv_label_set_text(feed_lbl, "Feed:");
    lv_obj_set_style_text_font(feed_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(feed_lbl, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(feed_lbl, right_x, start_y + UI_SCALE_Y(8));

    ta_feed = lv_textarea_create(tab);
    lv_obj_set_size(ta_feed, PF_FIELD_W, PF_FIELD_H);
    lv_obj_set_pos(ta_feed, right_x + UI_SCALE_X(80), start_y);
    lv_textarea_set_one_line(ta_feed, true);
    lv_textarea_set_accepted_chars(ta_feed, "0123456789");
    lv_textarea_set_text(ta_feed, "1000");
    lv_obj_set_style_text_font(ta_feed, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(ta_feed, onTextareaFocused, LV_EVENT_FOCUSED, nullptr);

    lv_obj_t *mode_lbl = lv_label_create(tab);
    lv_label_set_text(mode_lbl, "Absolute:");
    lv_obj_set_style_text_font(mode_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(mode_lbl, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(mode_lbl, right_x, start_y + PF_FIELD_H + row_gap + UI_SCALE_Y(6));

    mode_switch = lv_switch_create(tab);
    lv_obj_set_pos(mode_switch, right_x + UI_SCALE_X(120), start_y + PF_FIELD_H + row_gap);
    lv_obj_add_state(mode_switch, LV_STATE_CHECKED);

    lv_obj_t *encoder_lbl = lv_label_create(tab);
    lv_label_set_text(encoder_lbl, "Encoder:");
    lv_obj_set_style_text_font(encoder_lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(encoder_lbl, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(encoder_lbl, right_x, start_y + 2 * (PF_FIELD_H + row_gap) - UI_SCALE_Y(6));

    encoder_switch = lv_switch_create(tab);
    lv_obj_set_pos(encoder_switch, right_x + UI_SCALE_X(120), start_y + 2 * (PF_FIELD_H + row_gap) - UI_SCALE_Y(10));
    lv_obj_add_event_cb(encoder_switch, onEncoderToggle, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *btn_go = lv_button_create(tab);
    lv_obj_set_size(btn_go, UI_SCALE_X(160), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_go, right_x, start_y + 3 * (PF_FIELD_H + row_gap) + UI_SCALE_Y(10));
    lv_obj_set_style_bg_color(btn_go, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(btn_go, onGoPressed, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_go_lbl = lv_label_create(btn_go);
    lv_label_set_text(btn_go_lbl, "Go");
    lv_obj_set_style_text_font(btn_go_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(btn_go_lbl);

    lv_obj_t *btn_current = lv_button_create(tab);
    lv_obj_set_size(btn_current, UI_SCALE_X(160), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_current, right_x + UI_SCALE_X(180), start_y + 3 * (PF_FIELD_H + row_gap) + UI_SCALE_Y(10));
    lv_obj_set_style_bg_color(btn_current, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_current, onCurrentPressed, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_current_lbl = lv_label_create(btn_current);
    lv_label_set_text(btn_current_lbl, "Current");
    lv_obj_set_style_text_font(btn_current_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(btn_current_lbl);

    lv_obj_t *btn_clear = lv_button_create(tab);
    lv_obj_set_size(btn_clear, UI_SCALE_X(160), UI_SCALE_Y(50));
    lv_obj_set_pos(btn_clear, right_x + UI_SCALE_X(360), start_y + 3 * (PF_FIELD_H + row_gap) + UI_SCALE_Y(10));
    lv_obj_set_style_bg_color(btn_clear, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_clear, onClearPressed, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_clear_lbl = lv_label_create(btn_clear);
    lv_label_set_text(btn_clear_lbl, "Clear");
    lv_obj_set_style_text_font(btn_clear_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(btn_clear_lbl);

    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(status_label, UI_SCALE_X(10), UI_SCALE_Y(310));

    if (!encoder_timer) {
        encoder_timer = lv_timer_create(encoderTimerCb, 50, nullptr);
    }
}

void UITabControlPowerFeed::onGoPressed(lv_event_t *e) {
    LV_UNUSED(e);
    if (!CommManager::isConnected()) {
        setStatus("Not connected to machine.", UITheme::STATE_ALARM);
        return;
    }

    const char *x_text = lv_textarea_get_text(ta_x);
    const char *y_text = lv_textarea_get_text(ta_y);
    const char *z_text = lv_textarea_get_text(ta_z);
    const char *feed_text = lv_textarea_get_text(ta_feed);
    int feed = atoi(feed_text);
    if (feed <= 0) {
        setStatus("Invalid feed rate.", UITheme::STATE_ALARM);
        return;
    }

    bool has_axis = (x_text[0] != '\0') || (y_text[0] != '\0') || (z_text[0] != '\0');
    if (!has_axis) {
        setStatus("Enter at least one axis value.", UITheme::STATE_ALARM);
        return;
    }

    const FluidNCStatus &status = CommManager::getStatus();
    const char *units = status.modal_units[0] != '\0' ? status.modal_units : "G21";
    bool absolute = lv_obj_has_state(mode_switch, LV_STATE_CHECKED);

    char cmd[128];
    size_t len = 0;
    len += snprintf(cmd + len, sizeof(cmd) - len, "$J=%s%s", units, absolute ? "G90" : "G91");
    if (x_text[0] != '\0') len += snprintf(cmd + len, sizeof(cmd) - len, " X%s", x_text);
    if (y_text[0] != '\0') len += snprintf(cmd + len, sizeof(cmd) - len, " Y%s", y_text);
    if (z_text[0] != '\0') len += snprintf(cmd + len, sizeof(cmd) - len, " Z%s", z_text);
    snprintf(cmd + len, sizeof(cmd) - len, " F%d\n", feed);

    CommManager::sendCommand(cmd);
    setStatus("Command sent.", UITheme::STATE_IDLE);
}

void UITabControlPowerFeed::onCurrentPressed(lv_event_t *e) {
    LV_UNUSED(e);
    const FluidNCStatus &status = CommManager::getStatus();
    bool absolute = lv_obj_has_state(mode_switch, LV_STATE_CHECKED);
    char buf[32];

    if (absolute) {
        snprintf(buf, sizeof(buf), "%.3f", status.wpos_x);
        lv_textarea_set_text(ta_x, buf);
        snprintf(buf, sizeof(buf), "%.3f", status.wpos_y);
        lv_textarea_set_text(ta_y, buf);
        snprintf(buf, sizeof(buf), "%.3f", status.wpos_z);
        lv_textarea_set_text(ta_z, buf);
        setStatus("Loaded current position.", UITheme::STATE_IDLE);
    } else {
        lv_textarea_set_text(ta_x, "0");
        lv_textarea_set_text(ta_y, "0");
        lv_textarea_set_text(ta_z, "0");
        setStatus("Cleared to zero offsets.", UITheme::STATE_IDLE);
    }
}

void UITabControlPowerFeed::onClearPressed(lv_event_t *e) {
    LV_UNUSED(e);
    lv_textarea_set_text(ta_x, "");
    lv_textarea_set_text(ta_y, "");
    lv_textarea_set_text(ta_z, "");
    setStatus("Cleared axis values.", UITheme::STATE_IDLE);
}

void UITabControlPowerFeed::setStatus(const char *text, lv_color_t color) {
    if (!status_label) {
        return;
    }
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_color(status_label, color, 0);
}

void UITabControlPowerFeed::onTextareaFocused(lv_event_t *e) {
    lv_obj_t *ta = static_cast<lv_obj_t *>(lv_event_get_target(e));
    active_field = ta;
    if (encoder_enabled) {
        last_encoder_count = get_encoder_value(get_active_encoder_index(active_field));
        return;
    }
    showKeyboard(ta);
}

void UITabControlPowerFeed::showKeyboard(lv_obj_t *ta) {
    if (!keyboard) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(keyboard, SCREEN_WIDTH, UI_SCALE_Y(220));
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabControlPowerFeed::hideKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabControlPowerFeed::hideKeyboard(); }, LV_EVENT_CANCEL, nullptr);
        if (parent_tab) {
            lv_obj_add_event_cb(parent_tab, [](lv_event_t *e) { UITabControlPowerFeed::hideKeyboard(); }, LV_EVENT_CLICKED, nullptr);
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

void UITabControlPowerFeed::hideKeyboard() {
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (parent_tab) {
        lv_obj_set_style_pad_bottom(parent_tab, 0, 0);
        lv_obj_scroll_to_y(parent_tab, 0, LV_ANIM_OFF);
    }
}

void UITabControlPowerFeed::onEncoderToggle(lv_event_t *e) {
    LV_UNUSED(e);
    encoder_enabled = lv_obj_has_state(encoder_switch, LV_STATE_CHECKED);
    if (encoder_enabled) {
        last_encoder_count = get_encoder_value(get_active_encoder_index(active_field));
        hideKeyboard();
    }
}

void UITabControlPowerFeed::encoderTimerCb(lv_timer_t *timer) {
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
