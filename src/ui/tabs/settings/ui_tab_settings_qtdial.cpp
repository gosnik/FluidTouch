#include "ui/tabs/settings/ui_tab_settings_qtdial.h"

#include "core/qtdial_button_mapping.h"
#include "ui/tabs/ui_tab_settings.h"
#include "ui/ui_tabs.h"
#include "ui/ui_theme.h"
#include "config.h"

#include <cstdio>
#include <cstring>

namespace {

lv_obj_t *parent_tab = nullptr;
lv_obj_t *mapping_list = nullptr;
lv_obj_t *status_label = nullptr;
lv_timer_t *refresh_timer = nullptr;
uint32_t last_status_version = 0;
lv_obj_t *mapping_rows[static_cast<size_t>(QtdialButtonMappingTarget::Count)] = {};
lv_obj_t *highlighted_row = nullptr;

void apply_row_highlight(lv_obj_t *row, bool active) {
    if (!row || !lv_obj_is_valid(row)) {
        return;
    }
    lv_obj_set_style_border_width(row, active ? UI_SCALE_X(2) : 0, 0);
    lv_obj_set_style_border_color(row, UITheme::ACCENT_SECONDARY, 0);
}

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

void rebuild_mapping_list() {
    if (!mapping_list) {
        return;
    }

    const lv_coord_t previous_scroll_y = lv_obj_get_scroll_y(mapping_list);
    lv_obj_clean(mapping_list);
    memset(mapping_rows, 0, sizeof(mapping_rows));
    highlighted_row = nullptr;
    lv_obj_set_style_pad_row(mapping_list, UI_SCALE_Y(8), 0);

    for (size_t i = 0; i < QtdialButtonMappingManager::targetCount(); ++i) {
        const auto target = static_cast<QtdialButtonMappingTarget>(i);

        lv_obj_t *row = lv_obj_create(mapping_list);
        mapping_rows[i] = row;
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
        const bool is_learning_target =
            QtdialButtonMappingManager::isLearning() &&
            QtdialButtonMappingManager::learningTarget() == target;
        lv_obj_set_style_bg_color(learn_btn,
                                  is_learning_target ? UITheme::STATE_HOLD : UITheme::ACCENT_PRIMARY,
                                  0);
        lv_obj_add_event_cb(learn_btn, [](lv_event_t *e) {
            const auto target = static_cast<QtdialButtonMappingTarget>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
            if (QtdialButtonMappingManager::isLearning() &&
                QtdialButtonMappingManager::learningTarget() == target) {
                QtdialButtonMappingManager::cancelLearning();
            } else {
                QtdialButtonMappingManager::beginLearning(target);
            }
        }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_t *learn_lbl = lv_label_create(learn_btn);
        lv_label_set_text(learn_lbl, is_learning_target ? "Learning" : "Learn");
        lv_obj_set_style_text_font(learn_lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(learn_lbl);

        lv_obj_t *remove_btn = lv_button_create(row);
        lv_obj_set_size(remove_btn, UI_SCALE_X(90), UI_SCALE_Y(38));
        lv_obj_set_pos(remove_btn, UI_SCALE_X(510), UI_SCALE_Y(10));
        lv_obj_set_style_bg_color(remove_btn, UITheme::BG_BUTTON, 0);
        lv_obj_add_event_cb(remove_btn, [](lv_event_t *e) {
            const auto target = static_cast<QtdialButtonMappingTarget>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
            QtdialButtonMappingManager::setTargetActive(target, false);
            rebuild_mapping_list();
            refresh_status();
        }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_t *remove_lbl = lv_label_create(remove_btn);
        lv_label_set_text(remove_lbl, "Clear");
        lv_obj_set_style_text_font(remove_lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(remove_lbl);
    }

    lv_obj_update_layout(mapping_list);
    lv_obj_scroll_to_y(mapping_list, previous_scroll_y, LV_ANIM_OFF);
}

void refresh_timer_cb(lv_timer_t *) {
    const uint32_t version = QtdialButtonMappingManager::getStatusVersion();
    if (version == last_status_version) {
        return;
    }
    rebuild_mapping_list();
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
    lv_label_set_text(intro, "Tap Learn on a mapping row, then press the physical qtdial button. Clear removes the stored mapping for that row.");
    lv_obj_set_style_text_font(intro, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(intro, UITheme::TEXT_MEDIUM, 0);
    lv_obj_set_width(intro, UI_SCALE_X(690));
    lv_obj_set_pos(intro, UI_SCALE_X(20), UI_SCALE_Y(48));

    mapping_list = lv_obj_create(tab);
    lv_obj_set_size(mapping_list, UI_SCALE_X(690), UI_SCALE_Y(270));
    lv_obj_set_pos(mapping_list, UI_SCALE_X(20), UI_SCALE_Y(88));
    lv_obj_set_style_bg_color(mapping_list, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(mapping_list, 0, 0);
    lv_obj_set_style_pad_all(mapping_list, UI_SCALE_X(4), 0);
    lv_obj_set_style_pad_bottom(mapping_list, UI_SCALE_Y(10), 0);
    lv_obj_set_layout(mapping_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mapping_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(mapping_list, LV_DIR_VER);

    status_label = lv_label_create(tab);
    lv_obj_set_width(status_label, UI_SCALE_X(690));
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(status_label, UI_SCALE_X(20), UI_SCALE_Y(370));

    rebuild_mapping_list();
    refresh_status();

    if (!refresh_timer) {
        refresh_timer = lv_timer_create(refresh_timer_cb, 100, nullptr);
    }
}

bool UITabSettingsQtdial::isActive() {
    return parent_tab &&
           lv_obj_is_valid(parent_tab) &&
           UITabs::getActiveTab() == 5 &&
           UITabSettings::getActiveSubtab() == 3;
}

bool UITabSettingsQtdial::scrollToTarget(QtdialButtonMappingTarget target) {
    if (!isActive() || !mapping_list) {
        return false;
    }

    const size_t index = static_cast<size_t>(target);
    if (index >= static_cast<size_t>(QtdialButtonMappingTarget::Count)) {
        return false;
    }

    lv_obj_t *row = mapping_rows[index];
    if (!row || !lv_obj_is_valid(row)) {
        return false;
    }

    if (highlighted_row && highlighted_row != row) {
        apply_row_highlight(highlighted_row, false);
    }
    highlighted_row = row;
    apply_row_highlight(highlighted_row, true);
    lv_obj_scroll_to_view(row, LV_ANIM_ON);
    return true;
}
