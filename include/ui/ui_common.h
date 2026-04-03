#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <lvgl.h>
#include "config.h"

// Forward declaration
class DisplayDriver;

// UI state and shared objects
class UICommon {
public:
    using KeyboardShowFn = void (*)(lv_obj_t *ta);
    using KeyboardHideFn = void (*)();

    static void init(lv_display_t *disp);
    static void setDisplayDriver(DisplayDriver* driver);  // Set display driver reference
    static void createMainUI();  // Creates main UI screen, status bar, and tabs
    static void createStatusBar();
    
    // Update functions for status bar
    static void updateModalStates(const char *text);
    static void updateMachinePosition(float x, float y, float z);
    static void updateWorkPosition(float x, float y, float z);
    static void updateMachineState(const char *state);
    static void updateConnectionStatus(bool machine_connected, bool wifi_connected);
    static int getEncoderBindAxis();
    static void setEncoderBindAxis(int axis, bool force_display = false);
    static bool isEncoderBindEnabled();
    static void setEncoderBindEnabled(bool enabled, bool force_display = false);
    static bool isEncoderBindVisible();
    static void updateEncoderBindVisibility();
    static void maybeSendEncoderBindDisplay(bool force_display = false);
    static bool isOnScreenKeyboardEnabled();
    static void setOnScreenKeyboardEnabled(bool enabled);
    static void toggleOnScreenKeyboardEnabled();
    static void registerKeyboardTarget(lv_obj_t *ta, KeyboardShowFn show_fn, KeyboardHideFn hide_fn);
    static void clearKeyboardTarget(lv_obj_t *ta = nullptr);
    
    // Dialog functions
    static void showMachineSelectConfirmDialog();
    static void hideMachineSelectConfirmDialog();
    static void showPowerOffConfirmDialog();
    static void showConnectingPopup(const char *machine_name, const char *ssid);
    static void hideConnectingPopup();
    static void showConnectionErrorDialog(const char *title, const char *message, bool force_show = false);
    static void hideConnectionErrorDialog();
    static void checkConnectionTimeout();  // Non-blocking timeout check
    
    // State popup functions (HOLD and ALARM)
    static void showHoldPopup(const char *message);
    static void hideHoldPopup();
    static void showAlarmPopup(const char *message);
    static void hideAlarmPopup();
    static void checkStatePopups(int current_state, const char *last_message);  // Called from main loop
    
    // Getters for shared objects
    static lv_obj_t* getStatusBar() { return status_bar; }
    static lv_display_t* getDisplay() { return display; }
    static DisplayDriver* getDisplayDriver() { return display_driver; }
    
private:
    static lv_display_t *display;
    static DisplayDriver *display_driver;
    static lv_obj_t *status_bar;
    static lv_obj_t *status_bar_left_area;   // Clickable area for Status tab
    static lv_obj_t *status_bar_right_area;  // Clickable area for machine selection
    static lv_obj_t *machine_select_dialog;  // Confirmation dialog
    static lv_obj_t *connecting_popup;       // Connecting popup
    static lv_obj_t *connection_error_dialog; // Connection error dialog
    static lv_obj_t *hold_popup;             // HOLD state popup
    static lv_obj_t *alarm_popup;            // ALARM state popup
    static int last_popup_state;             // Track last state to detect changes
    static bool hold_popup_dismissed;        // User dismissed HOLD popup
    static bool alarm_popup_dismissed;       // User dismissed ALARM popup
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

    static lv_obj_t *encoder_bind_container;
    static lv_obj_t *encoder_bind_buttons[3];
    static int encoder_bind_axis;
    static bool encoder_bind_enabled;
    static bool encoder_bind_visible;
    static uint32_t last_bind_display_ms;
    static bool keyboard_toggle_overridden;
    static bool keyboard_toggle_enabled;
    static lv_obj_t *keyboard_target;
    static KeyboardShowFn keyboard_show_fn;
    static KeyboardHideFn keyboard_hide_fn;
    
    // Cached values for delta checking (prevent unnecessary redraws)
    static float last_wpos_x, last_wpos_y, last_wpos_z;
    static float last_mpos_x, last_mpos_y, last_mpos_z;
};

#endif // UI_COMMON_H
