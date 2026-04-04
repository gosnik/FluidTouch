#ifndef UI_TAB_CONTROL_H
#define UI_TAB_CONTROL_H

#include <lvgl.h>

class UITabControl {
public:
    static void create(lv_obj_t *tab);
    static void setActiveSubtab(uint32_t index, lv_anim_enable_t anim = LV_ANIM_OFF);
    static uint32_t getActiveSubtab();
    static uint32_t getSubtabCount();
    static lv_obj_t *getActivePage();
    static void cycleSubtabs();

private:
    static lv_obj_t *sub_tabview;
};

#endif // UI_TAB_CONTROL_H
