#include "ui/tabs/settings/ui_tab_settings_qtdial.h"

#include "core/qtdial_button_mapping.h"
#include "ui/ui_theme.h"
#include "config.h"

#include <cstdio>
#include <cstring>

namespace {

lv_obj_t *parent_tab = nullptr;
lv_obj_t *target_dropdown = nullptr;
lv_obj_t *mapping_list = nullptr;
lv_obj_t *status_label = nullptr;
lv_timer_t *refresh_timer = nullptr;
uint32_t last_status_version = 0;

void refresh_status() {
    const uint32_t version = QtdialButtonMappingManager::getStatusVersion();
    if (version == last_status_version) {
        return;
    }
    last_status_version = version;
    if (status_label) {
        lv_label_set_text(status_label, QtdialButtonMappingManager::getStatusMessage());
    }
}

void rebuild_target_dropdown() {
    if (!target_dropdown) {
        return;
    }

    char options[2048] = {};
    bool first = true;
    for (size_t i = 0; i < QtdialButtonMappingManager::targetCount(); ++i) {
        const auto target = static_cast<QtdialButtonMappingTarget>(i);
        if (QtdialButtonMappingManager::isTargetActive(target)) {
            continue;
        }
        if (!first) {
            strncat(options, "\n", sizeof(options) - strlen(options) - 1);
        }
        strncat(options, QtdialButtonMappingManager::targetLabel(target), sizeof(options) - strlen(options) - 1);
        first = false;
    }

    if (first) {
        snprintf(options, sizeof(options), "No targets available");
    }
    lv_dropdown_set_options(target_dropdown, options);
    if (first) {
        lv_obj_add_state(target_dropdown, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(target_dropdown, LV_STATE_DISABLED);
    }
}

void rebuild_mapping_list() {
    if (!mapping_list) {
        return;
    }

    lv_obj_clean(mapping_list);
    lv_obj_set_style_pad_row(mapping_list, UI_SCALE_Y(8), 0);

    for (size_t i = 0; i < QtdialButtonMappingManager::targetCount(); ++i) {
        const auto target = static_cast<QtdialButtonMappingTarget>(i);
        if (!QtdialButtonMappingManager::isTargetActive(target)) {
            continue;
        }

        lv_obj_t *row = lv_obj_create(mapping_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, UI_SCALE_Y(58));
        lv_obj_set_style_bg_color(row, UITheme::BG_DARKER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, UI_SCALE_X(8), 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, QtdialButtonMappingManager::targetLabel(target));
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(label, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_pos(label, UI_SCALE_X(6), UI_SCALE_Y(4));

        lv_obj_t *map_label = lv_label_create(row);
        const int mapped_button = QtdialButtonMappingManager::getMappedButton(target);
        if (mapped_button >= 0) {
            lv_label_set_text_fmt(map_label, "Mapped: %s", QtdialButtonMappingManager::buttonLabel(static_cast<uint8_t>(mapped_button)));
        } else {
            lv_label_set_text(map_label, "Mapped: Unassigned");
        }
        lv_obj_set_style_text_font(map_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(map_label, UITheme::TEXT_MEDIUM, 0);
        lv_obj_set_pos(map_label, UI_SCALE_X(6), UI_SCALE_Y(30));

        lv_obj_t *learn_btn = lv_button_create(row);
        lv_obj_set_size(learn_btn, UI_SCALE_X(90), UI_SCALE_Y(38));
        lv_obj_set_pos(learn_btn, UI_SCALE_X(410), UI_SCALE_Y(10));
        lv_obj_set_style_bg_color(learn_btn, UITheme::ACCENT_PRIMARY, 0);
        lv_obj_add_event_cb(learn_btn, [](lv_event_t *e) {
            const auto target = static_cast<QtdialButtonMappingTarget>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
            QtdialButtonMappingManager::beginLearning(target);
        }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_t *learn_lbl = lv_label_create(learn_btn);
        lv_label_set_text(learn_lbl, "Learn");
        lv_obj_set_style_text_font(learn_lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(learn_lbl);

        lv_obj_t *remove_btn = lv_button_create(row);
        lv_obj_set_size(remove_btn, UI_SCALE_X(90), UI_SCALE_Y(38));
        lv_obj_set_pos(remove_btn, UI_SCALE_X(510), UI_SCALE_Y(10));
        lv_obj_set_style_bg_color(remove_btn, UITheme::BG_BUTTON, 0);
        lv_obj_add_event_cb(remove_btn, [](lv_event_t *e) {
            const auto target = static_cast<QtdialButtonMappingTarget>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
            QtdialButtonMappingManager::setTargetActive(target, false);
            rebuild_target_dropdown();
            rebuild_mapping_list();
            refresh_status();
        }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_t *remove_lbl = lv_label_create(remove_btn);
        lv_label_set_text(remove_lbl, "Remove");
        lv_obj_set_style_text_font(remove_lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(remove_lbl);
    }
}

void add_mapping_target(lv_event_t *) {
    if (!target_dropdown || lv_obj_has_state(target_dropdown, LV_STATE_DISABLED)) {
        return;
    }

    uint16_t selected = lv_dropdown_get_selected(target_dropdown);
    uint16_t visible_index = 0;
    for (size_t i = 0; i < QtdialButtonMappingManager::targetCount(); ++i) {
        const auto target = static_cast<QtdialButtonMappingTarget>(i);
        if (QtdialButtonMappingManager::isTargetActive(target)) {
            continue;
        }
        if (visible_index == selected) {
            QtdialButtonMappingManager::setTargetActive(target, true);
            rebuild_target_dropdown();
            rebuild_mapping_list();
            refresh_status();
            return;
        }
        visible_index++;
    }
}

void cancel_learning(lv_event_t *) {
    QtdialButtonMappingManager::cancelLearning();
    refresh_status();
}

void refresh_timer_cb(lv_timer_t *) {
    const uint32_t version = QtdialButtonMappingManager::getStatusVersion();
    if (version == last_status_version) {
        return;
    }
    rebuild_mapping_list();
    rebuild_target_dropdown();
    refresh_status();
}

}  // namespace

void UITabSettingsQtdial::create(lv_obj_t *tab) {
    parent_tab = tab;
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(tab);
    lv_label_set_text(title, "QTDIAL BUTTON MAPPINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(title, UI_SCALE_X(20), UI_SCALE_Y(15));

    lv_obj_t *intro = lv_label_create(tab);
    lv_label_set_text(intro, "Add a jog target, tap Learn, then press the physical qtdial button to assign it.");
    lv_obj_set_style_text_font(intro, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(intro, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_width(intro, UI_SCALE_X(620));
    lv_obj_set_pos(intro, UI_SCALE_X(20), UI_SCALE_Y(48));

    target_dropdown = lv_dropdown_create(tab);
    lv_obj_set_size(target_dropdown, UI_SCALE_X(260), UI_SCALE_Y(44));
    lv_obj_set_pos(target_dropdown, UI_SCALE_X(20), UI_SCALE_Y(88));
    lv_obj_set_style_text_font(target_dropdown, &lv_font_montserrat_16, 0);

    lv_obj_t *add_btn = lv_button_create(tab);
    lv_obj_set_size(add_btn, UI_SCALE_X(120), UI_SCALE_Y(44));
    lv_obj_set_pos(add_btn, UI_SCALE_X(292), UI_SCALE_Y(88));
    lv_obj_set_style_bg_color(add_btn, UITheme::BTN_PLAY, 0);
    lv_obj_add_event_cb(add_btn, add_mapping_target, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *add_lbl = lv_label_create(add_btn);
    lv_label_set_text(add_lbl, "Add Target");
    lv_obj_set_style_text_font(add_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(add_lbl);

    lv_obj_t *cancel_btn = lv_button_create(tab);
    lv_obj_set_size(cancel_btn, UI_SCALE_X(120), UI_SCALE_Y(44));
    lv_obj_set_pos(cancel_btn, UI_SCALE_X(424), UI_SCALE_Y(88));
    lv_obj_set_style_bg_color(cancel_btn, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(cancel_btn, cancel_learning, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel Learn");
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(cancel_lbl);

    mapping_list = lv_obj_create(tab);
    lv_obj_set_size(mapping_list, UI_SCALE_X(690), UI_SCALE_Y(230));
    lv_obj_set_pos(mapping_list, UI_SCALE_X(20), UI_SCALE_Y(145));
    lv_obj_set_style_bg_color(mapping_list, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(mapping_list, 0, 0);
    lv_obj_set_style_pad_all(mapping_list, UI_SCALE_X(4), 0);
    lv_obj_set_layout(mapping_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mapping_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(mapping_list, LV_DIR_VER);

    status_label = lv_label_create(tab);
    lv_obj_set_width(status_label, UI_SCALE_X(690));
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(status_label, UI_SCALE_X(20), UI_SCALE_Y(388));

    rebuild_target_dropdown();
    rebuild_mapping_list();
    refresh_status();

    if (!refresh_timer) {
        refresh_timer = lv_timer_create(refresh_timer_cb, 100, nullptr);
    }
}
