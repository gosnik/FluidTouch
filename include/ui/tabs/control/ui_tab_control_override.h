#ifndef UI_TAB_CONTROL_OVERRIDE_H
#define UI_TAB_CONTROL_OVERRIDE_H

#include <lvgl.h>

class UITabControlOverride {
public:
    static lv_obj_t* create(lv_obj_t* parent);
    static void updateValues(float feed_ovr, float rapid_ovr, float spindle_ovr);
    
private:
    static lv_obj_t* lbl_feed_value;
    static lv_obj_t* lbl_rapid_value;
    static lv_obj_t* lbl_spindle_value;
};

#endif // UI_TAB_CONTROL_OVERRIDE_H
