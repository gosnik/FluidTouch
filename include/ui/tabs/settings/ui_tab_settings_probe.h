#ifndef UI_TAB_SETTINGS_PROBE_H
#define UI_TAB_SETTINGS_PROBE_H

#include <lvgl.h>

class UITabSettingsProbe {
public:
    static void create(lv_obj_t *tab);
    static void loadPreferences();
    static void savePreferences();
    
    // Getters
    static int getDefaultFeedRate();
    static int getDefaultMaxDistance();
    static int getDefaultRetract();
    static float getDefaultThickness();
    
    // Setters
    static void setDefaultFeedRate(int value);
    static void setDefaultMaxDistance(int value);
    static void setDefaultRetract(int value);
    static void setDefaultThickness(float value);
    
    // Keyboard support
    static lv_obj_t* keyboard;
    static lv_obj_t* parent_tab;
    static void showKeyboard(lv_obj_t *ta);
    static void hideKeyboard();

private:
    static int default_feed_rate;
    static int default_max_distance;
    static int default_retract;
    static float default_thickness;
};

#endif // UI_TAB_SETTINGS_PROBE_H
