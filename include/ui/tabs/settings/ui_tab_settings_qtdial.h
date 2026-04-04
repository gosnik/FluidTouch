#ifndef UI_TAB_SETTINGS_QTDIAL_H
#define UI_TAB_SETTINGS_QTDIAL_H

#include "core/qtdial_button_mapping.h"
#include <lvgl.h>

class UITabSettingsQtdial {
public:
    static void create(lv_obj_t *tab);
    static bool isActive();
    static bool scrollToTarget(QtdialButtonMappingTarget target);
};

#endif // UI_TAB_SETTINGS_QTDIAL_H
