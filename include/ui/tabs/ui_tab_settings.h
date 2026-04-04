#ifndef UI_TAB_SETTINGS_H
#define UI_TAB_SETTINGS_H

#include <lvgl.h>

class UITabSettings {
public:
    static void create(lv_obj_t *tab);
    static uint32_t getActiveSubtab();
private:
    static lv_obj_t *sub_tabview;
    static void createGeneralTab(lv_obj_t *tab);
    static void createPowerTab(lv_obj_t *tab);
    static void createJogTab(lv_obj_t *tab);
    static void createQtdialTab(lv_obj_t *tab);
    static void createProbeTab(lv_obj_t *tab);
    static void createAboutTab(lv_obj_t *tab);
};

#endif // UI_TAB_SETTINGS_H
