#ifndef UI_TAB_STATUS_H
#define UI_TAB_STATUS_H

#include <lvgl.h>

class UITabStatus {
public:
    static void create(lv_obj_t *tab);
    
    // Update methods for live data
    static void updateMessage(const char *message);
    static void updateState(const char *state);
    static void updateWorkPosition(float x, float y, float z);
    static void updateMachinePosition(float x, float y, float z);
    static void updateFeedRate(float rate, float override_pct);
    static void updateRapidOverride(float override_pct);
    static void updateSpindle(float speed, float override_pct);
    static void updateModalStates(const char *wcs, const char *plane, const char *dist, 
                                  const char *units, const char *motion, const char *feedrate,
                                  const char *spindle, const char *coolant, const char *tool);
    static void updateFileProgress(bool is_printing, float percent, const char *filename, 
                                   uint32_t elapsed_ms);
    
private:
    static lv_obj_t *lbl_message;
    static lv_obj_t *lbl_state;
    static lv_obj_t *lbl_file_progress_container;  // Container for file progress section
    static lv_obj_t *lbl_filename;
    static lv_obj_t *bar_progress;
    static lv_obj_t *lbl_percent;
    static lv_obj_t *lbl_elapsed_time;
    static lv_obj_t *lbl_elapsed_unit;
    static lv_obj_t *lbl_estimated_time;
    static lv_obj_t *lbl_estimated_unit;
    static lv_obj_t *lbl_wpos_x;
    static lv_obj_t *lbl_wpos_y;
    static lv_obj_t *lbl_wpos_z;
    static lv_obj_t *lbl_mpos_x;
    static lv_obj_t *lbl_mpos_y;
    static lv_obj_t *lbl_mpos_z;
    static lv_obj_t *lbl_feed_value;
    static lv_obj_t *lbl_feed_override;
    static lv_obj_t *lbl_feed_units;
    static lv_obj_t *lbl_rapid_override;
    static lv_obj_t *lbl_spindle_value;
    static lv_obj_t *lbl_spindle_override;
    static lv_obj_t *lbl_spindle_units;
    
    // Modal state labels
    static lv_obj_t *lbl_modal_wcs;
    static lv_obj_t *lbl_modal_plane;
    static lv_obj_t *lbl_modal_dist;
    static lv_obj_t *lbl_modal_units;
    static lv_obj_t *lbl_modal_motion;
    static lv_obj_t *lbl_modal_feedrate;
    static lv_obj_t *lbl_modal_spindle;
    static lv_obj_t *lbl_modal_coolant;
    static lv_obj_t *lbl_modal_tool;
    
    // Cached values for delta checking (prevent unnecessary redraws)
    static float last_wpos_x, last_wpos_y, last_wpos_z;
    static float last_mpos_x, last_mpos_y, last_mpos_z;
    static float last_feed_rate, last_feed_override;
    static float last_rapid_override;
    static float last_spindle_speed, last_spindle_override;
    static char last_state[16];
    static char last_modal_wcs[8];
    static char last_modal_plane[8];
    static char last_modal_dist[8];
    static char last_modal_units[8];
    static char last_modal_motion[8];
    static char last_modal_feedrate[8];
    static char last_modal_spindle[8];
    static char last_modal_coolant[8];
    static char last_modal_tool[8];
};

#endif // UI_TAB_STATUS_H
