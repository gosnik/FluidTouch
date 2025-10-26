#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <lvgl.h>
#include "config.h"

// UI state and shared objects
class UICommon {
public:
    static void init(lv_display_t *disp);
    static void createMainUI();  // Creates main UI screen, status bar, and tabs
    static void createStatusBar();
    
    // Update functions for status bar
    static void updateModalStates(const char *text);
    static void updateMachinePosition(float x, float y, float z);
    static void updateWorkPosition(float x, float y, float z);
    static void updateMachineState(const char *state);
    static void updateConnectionStatus(bool machine_connected, bool wifi_connected);
    
    // Dialog functions
    static void showMachineSelectConfirmDialog();
    static void hideMachineSelectConfirmDialog();
    
    // Getters for shared objects
    static lv_obj_t* getStatusBar() { return status_bar; }
    static lv_display_t* getDisplay() { return display; }
    
private:
    static lv_display_t *display;
    static lv_obj_t *status_bar;
    static lv_obj_t *status_bar_left_area;   // Clickable area for Status tab
    static lv_obj_t *status_bar_right_area;  // Clickable area for machine selection
    static lv_obj_t *machine_select_dialog;  // Confirmation dialog
    static lv_obj_t *lbl_modal_states;
    static lv_obj_t *lbl_status;
    
    // Connection status labels (symbols only)
    static lv_obj_t *lbl_machine_symbol;
    static lv_obj_t *lbl_machine_name;
    static lv_obj_t *lbl_wifi_symbol;
    static lv_obj_t *lbl_wifi_name;
    
    // Work Position labels (individual axes)
    static lv_obj_t *lbl_wpos_label;
    static lv_obj_t *lbl_wpos_x;
    static lv_obj_t *lbl_wpos_y;
    static lv_obj_t *lbl_wpos_z;
    
    // Machine Position labels (individual axes)
    static lv_obj_t *lbl_mpos_label;
    static lv_obj_t *lbl_mpos_x;
    static lv_obj_t *lbl_mpos_y;
    static lv_obj_t *lbl_mpos_z;
    
    // Cached values for delta checking (prevent unnecessary redraws)
    static float last_wpos_x, last_wpos_y, last_wpos_z;
    static float last_mpos_x, last_mpos_y, last_mpos_z;
};

#endif // UI_COMMON_H
