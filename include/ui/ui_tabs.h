#ifndef UI_TABS_H
#define UI_TABS_H

#include <lvgl.h>
#include "config.h"

class UITabs {
public:
    static void createTabs();
    static void updateKeyboardToggleButton();
    static void createStatusTab(lv_obj_t *tab);
    static void createControlTab(lv_obj_t *tab);
    static void createFilesTab(lv_obj_t *tab);
    static void createMacrosTab(lv_obj_t *tab);
    static void createTerminalTab(lv_obj_t *tab);
    static void createSettingsTab(lv_obj_t *tab);
    static void updateMacrosTabRecordingIndicator();
    static void setActiveTab(uint32_t index, lv_anim_enable_t anim = LV_ANIM_OFF);
    static uint32_t getActiveTab();
    static uint32_t getTabCount();
    static void cycleTabs();
    
    // Settings management
    static void loadSettings();
    static void saveSettings();
    
    // Getters for tab objects (for external access if needed)
    static lv_obj_t* getTabview() { return tabview; }
    
private:
    static lv_obj_t *tabview;
    static lv_obj_t *tab_status;
    static lv_obj_t *tab_control;
    static lv_obj_t *tab_files;
    static lv_obj_t *tab_macros;
    static lv_obj_t *tab_terminal;
    static lv_obj_t *tab_settings;
    static lv_obj_t *keyboard_toggle_btn;
    static lv_obj_t *keyboard_toggle_label;
    static lv_timer_t *macros_record_indicator_timer;
    
    // Event handler for tab changes
    static void tab_changed_event_cb(lv_event_t *e);
};

#endif // UI_TABS_H
