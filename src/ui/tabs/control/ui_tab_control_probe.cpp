#include "ui/tabs/control/ui_tab_control_probe.h"
#include "ui/tabs/settings/ui_tab_settings_probe.h"
#include "ui/ui_theme.h"
#include "fluidnc_client.h"
#include "config.h"
#include <lvgl.h>

// Static member for results display
lv_obj_t* UITabControlProbe::results_text = nullptr;

// Keyboard support
lv_obj_t* UITabControlProbe::keyboard = nullptr;
lv_obj_t* UITabControlProbe::parent_tab = nullptr;

// Static pointers to input fields for reading values
static lv_obj_t* feed_input_ptr = nullptr;
static lv_obj_t* dist_input_ptr = nullptr;
static lv_obj_t* retract_input_ptr = nullptr;
static lv_obj_t* thickness_input_ptr = nullptr;

// Forward declaration for event handler
static void textarea_focused_event_handler(lv_event_t *e);

void UITabControlProbe::create(lv_obj_t *parent) {
    // Store parent tab reference
    parent_tab = parent;
    
    // Load probe defaults from settings
    UITabSettingsProbe::loadPreferences();
    
    // Disable scrolling initially - will be enabled when keyboard appears
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    
    // === X-AXIS SECTION ===
    lv_obj_t* x_header = lv_label_create(parent);
    lv_label_set_text(x_header, "X-AXIS");
    lv_obj_set_style_text_font(x_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(x_header, UITheme::AXIS_X, 0);
    lv_obj_set_pos(x_header, 10, 10);
    
    // X- button
    lv_obj_t* x_minus_btn = lv_button_create(parent);
    lv_obj_set_size(x_minus_btn, 120, 50);
    lv_obj_set_pos(x_minus_btn, 10, 45);
    lv_obj_set_style_bg_color(x_minus_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(x_minus_btn, probe_x_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* x_minus_lbl = lv_label_create(x_minus_btn);
    lv_label_set_text(x_minus_lbl, "X-");
    lv_obj_set_style_text_font(x_minus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(x_minus_lbl);
    
    // X+ button
    lv_obj_t* x_plus_btn = lv_button_create(parent);
    lv_obj_set_size(x_plus_btn, 120, 50);
    lv_obj_set_pos(x_plus_btn, 140, 45);
    lv_obj_set_style_bg_color(x_plus_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(x_plus_btn, probe_x_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* x_plus_lbl = lv_label_create(x_plus_btn);
    lv_label_set_text(x_plus_lbl, "X+");
    lv_obj_set_style_text_font(x_plus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(x_plus_lbl);
    
    // === Y-AXIS SECTION ===
    lv_obj_t* y_header = lv_label_create(parent);
    lv_label_set_text(y_header, "Y-AXIS");
    lv_obj_set_style_text_font(y_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(y_header, UITheme::AXIS_Y, 0);
    lv_obj_set_pos(y_header, 10, 115);
    
    // Y- button
    lv_obj_t* y_minus_btn = lv_button_create(parent);
    lv_obj_set_size(y_minus_btn, 120, 50);
    lv_obj_set_pos(y_minus_btn, 10, 150);
    lv_obj_set_style_bg_color(y_minus_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(y_minus_btn, probe_y_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* y_minus_lbl = lv_label_create(y_minus_btn);
    lv_label_set_text(y_minus_lbl, "Y-");
    lv_obj_set_style_text_font(y_minus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(y_minus_lbl);
    
    // Y+ button
    lv_obj_t* y_plus_btn = lv_button_create(parent);
    lv_obj_set_size(y_plus_btn, 120, 50);
    lv_obj_set_pos(y_plus_btn, 140, 150);
    lv_obj_set_style_bg_color(y_plus_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(y_plus_btn, probe_y_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* y_plus_lbl = lv_label_create(y_plus_btn);
    lv_label_set_text(y_plus_lbl, "Y+");
    lv_obj_set_style_text_font(y_plus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(y_plus_lbl);
    
    // === Z-AXIS SECTION ===
    lv_obj_t* z_header = lv_label_create(parent);
    lv_label_set_text(z_header, "Z-AXIS");
    lv_obj_set_style_text_font(z_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(z_header, UITheme::AXIS_Z, 0);
    lv_obj_set_pos(z_header, 10, 220);
    
    // Z- button
    lv_obj_t* z_minus_btn = lv_button_create(parent);
    lv_obj_set_size(z_minus_btn, 120, 50);
    lv_obj_set_pos(z_minus_btn, 10, 255);
    lv_obj_set_style_bg_color(z_minus_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(z_minus_btn, probe_z_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* z_minus_lbl = lv_label_create(z_minus_btn);
    lv_label_set_text(z_minus_lbl, "Z-");
    lv_obj_set_style_text_font(z_minus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(z_minus_lbl);
    
    // Z+ button
    lv_obj_t* z_plus_btn = lv_button_create(parent);
    lv_obj_set_size(z_plus_btn, 120, 50);
    lv_obj_set_pos(z_plus_btn, 140, 255);
    lv_obj_set_style_bg_color(z_plus_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(z_plus_btn, probe_z_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* z_plus_lbl = lv_label_create(z_plus_btn);
    lv_label_set_text(z_plus_lbl, "Z+");
    lv_obj_set_style_text_font(z_plus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(z_plus_lbl);
    
    // === PARAMETERS SECTION (Right Side) ===
    lv_obj_t* params_header = lv_label_create(parent);
    lv_label_set_text(params_header, "PARAMETERS");
    lv_obj_set_style_text_font(params_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(params_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(params_header, 300, 10);
    
    // Feed Rate
    lv_obj_t* feed_label = lv_label_create(parent);
    lv_label_set_text(feed_label, "Feed Rate:");
    lv_obj_set_style_text_font(feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(feed_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(feed_label, 300, 57);  // +12 from field y for vertical centering
    
    feed_input_ptr = lv_textarea_create(parent);
    lv_textarea_set_one_line(feed_input_ptr, true);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", UITabSettingsProbe::getDefaultFeedRate());
    lv_textarea_set_text(feed_input_ptr, buf);
    lv_textarea_set_accepted_chars(feed_input_ptr, "0123456789");  // Integers only
    lv_obj_set_size(feed_input_ptr, 100, 40);
    lv_obj_set_pos(feed_input_ptr, 450, 45);  // Moved right by 40px
    lv_obj_clear_flag(feed_input_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(feed_input_ptr, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    
    lv_obj_t* feed_unit = lv_label_create(parent);
    lv_label_set_text(feed_unit, "mm/min");
    lv_obj_set_style_text_font(feed_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(feed_unit, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(feed_unit, 560, 57);  // +12 from field y for vertical centering
    
    // Max Distance
    lv_obj_t* dist_label = lv_label_create(parent);
    lv_label_set_text(dist_label, "Max Distance:");
    lv_obj_set_style_text_font(dist_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(dist_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(dist_label, 300, 107);  // Increased spacing (was 90, +50 vertical spacing)
    
    dist_input_ptr = lv_textarea_create(parent);
    lv_textarea_set_one_line(dist_input_ptr, true);
    snprintf(buf, sizeof(buf), "%d", UITabSettingsProbe::getDefaultMaxDistance());
    lv_textarea_set_text(dist_input_ptr, buf);
    lv_textarea_set_accepted_chars(dist_input_ptr, "0123456789");  // Integers only
    lv_obj_set_size(dist_input_ptr, 100, 40);
    lv_obj_set_pos(dist_input_ptr, 450, 95);  // Moved right by 40px, increased vertical spacing
    lv_obj_clear_flag(dist_input_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(dist_input_ptr, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    
    lv_obj_t* dist_unit = lv_label_create(parent);
    lv_label_set_text(dist_unit, "mm");
    lv_obj_set_style_text_font(dist_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(dist_unit, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(dist_unit, 560, 107);  // +12 from field y for vertical centering
    
    // Retract Distance
    lv_obj_t* retract_label = lv_label_create(parent);
    lv_label_set_text(retract_label, "Retract:");
    lv_obj_set_style_text_font(retract_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(retract_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(retract_label, 300, 157);  // Increased spacing (was 130, +50 vertical spacing)
    
    retract_input_ptr = lv_textarea_create(parent);
    lv_textarea_set_one_line(retract_input_ptr, true);
    snprintf(buf, sizeof(buf), "%d", UITabSettingsProbe::getDefaultRetract());
    lv_textarea_set_text(retract_input_ptr, buf);
    lv_textarea_set_accepted_chars(retract_input_ptr, "0123456789");  // Integers only
    lv_obj_set_size(retract_input_ptr, 100, 40);
    lv_obj_set_pos(retract_input_ptr, 450, 145);  // Moved right by 40px, increased vertical spacing
    lv_obj_clear_flag(retract_input_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(retract_input_ptr, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    
    lv_obj_t* retract_unit = lv_label_create(parent);
    lv_label_set_text(retract_unit, "mm");
    lv_obj_set_style_text_font(retract_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(retract_unit, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(retract_unit, 560, 157);  // +12 from field y for vertical centering
    
    // Probe Thickness (P parameter)
    lv_obj_t* thickness_label = lv_label_create(parent);
    lv_label_set_text(thickness_label, "Probe Thickness:");
    lv_obj_set_style_text_font(thickness_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(thickness_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(thickness_label, 300, 207);  // Increased spacing (was 170, +50 vertical spacing)
    
    thickness_input_ptr = lv_textarea_create(parent);
    lv_textarea_set_one_line(thickness_input_ptr, true);
    snprintf(buf, sizeof(buf), "%.1f", UITabSettingsProbe::getDefaultThickness());
    lv_textarea_set_text(thickness_input_ptr, buf);
    lv_textarea_set_accepted_chars(thickness_input_ptr, "0123456789.");  // Allow decimal point
    lv_obj_set_size(thickness_input_ptr, 100, 40);
    lv_obj_set_pos(thickness_input_ptr, 450, 195);  // Moved right by 40px, increased vertical spacing
    lv_obj_clear_flag(thickness_input_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(thickness_input_ptr, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    
    lv_obj_t* thickness_unit = lv_label_create(parent);
    lv_label_set_text(thickness_unit, "mm");
    lv_obj_set_style_text_font(thickness_unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(thickness_unit, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(thickness_unit, 560, 207);  // +12 from field y for vertical centering
    
    // === RESULTS SECTION ===
    lv_obj_t* results_header = lv_label_create(parent);
    lv_label_set_text(results_header, "PROBE RESULT");
    lv_obj_set_style_text_font(results_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(results_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(results_header, 300, 240);  // Moved up more to prevent cutoff
    
    results_text = lv_textarea_create(parent);
    lv_textarea_set_text(results_text, "No probe data");
    lv_obj_set_size(results_text, 330, 75);  // Slightly taller
    lv_obj_set_pos(results_text, 300, 275);  // Adjusted position
    lv_obj_clear_flag(results_text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(results_text, LV_OBJ_FLAG_SCROLLABLE);  // Prevent scrolling
}

// Event handlers
void UITabControlProbe::probe_x_minus_handler(lv_event_t* e) {
    executeProbe("X", "-");
}

void UITabControlProbe::probe_x_plus_handler(lv_event_t* e) {
    executeProbe("X", "+");
}

void UITabControlProbe::probe_y_minus_handler(lv_event_t* e) {
    executeProbe("Y", "-");
}

void UITabControlProbe::probe_y_plus_handler(lv_event_t* e) {
    executeProbe("Y", "+");
}

void UITabControlProbe::probe_z_minus_handler(lv_event_t* e) {
    executeProbe("Z", "-");
}

void UITabControlProbe::probe_z_plus_handler(lv_event_t* e) {
    executeProbe("Z", "+");
}

void UITabControlProbe::executeProbe(const char* axis, const char* direction) {
    if (!FluidNCClient::isConnected()) {
        if (results_text) {
            lv_textarea_set_text(results_text, "Error: Not connected to FluidNC");
        }
        return;
    }
    
    // Read parameters from input fields
    const char* feed_text = lv_textarea_get_text(feed_input_ptr);
    const char* dist_text = lv_textarea_get_text(dist_input_ptr);
    const char* retract_text = lv_textarea_get_text(retract_input_ptr);
    const char* thickness_text = lv_textarea_get_text(thickness_input_ptr);
    
    float feed_rate = atof(feed_text);
    float max_dist = atof(dist_text);
    float retract = atof(retract_text);
    float thickness = atof(thickness_text);
    
    // Validate inputs
    if (feed_rate <= 0 || max_dist <= 0) {
        if (results_text) {
            lv_textarea_set_text(results_text, "Error: Invalid feed rate or distance");
        }
        return;
    }
    
    // Build G38.2 command: G38.2 G91 <AXIS><DIRECTION><DISTANCE> F<FEED> P<THICKNESS>
    char command[128];
    
    if (thickness > 0.001) {  // Use P parameter if thickness is specified
        snprintf(command, sizeof(command), "G38.2 G91 %c%.1f F%.0f P%.2f", 
                 axis[0], (direction[0] == '-' ? -max_dist : max_dist), feed_rate, thickness);
    } else {
        snprintf(command, sizeof(command), "G38.2 G91 %c%.1f F%.0f", 
                 axis[0], (direction[0] == '-' ? -max_dist : max_dist), feed_rate);
    }
    
    // Update result text to show probing in progress
    if (results_text) {
        char status[64];
        snprintf(status, sizeof(status), "Probing %s%s...", axis, direction);
        lv_textarea_set_text(results_text, status);
    }
    
    // Save current distance mode (G90/G91) to restore after probing
    const char* saved_distance_mode = FluidNCClient::getStatus().modal_distance;
    
    Serial.printf("Probe: Sending command: %s\n", command);
    FluidNCClient::sendCommand(command);
    
    // Send retract move if retract distance is specified
    if (retract > 0.001) {
        // Retract in opposite direction from probe (no + sign in GCode)
        char retract_sign = (direction[0] == '-') ? ' ' : '-';
        
        // Send commands separately to ensure modal states persist correctly
        FluidNCClient::sendCommand("G91");  // Enter relative mode
        
        char retract_command[64];
        snprintf(retract_command, sizeof(retract_command), "G0 %c%c%.2f", 
                 axis[0], retract_sign, retract);
        Serial.printf("Probe: Sending retract: %s\n", retract_command);
        FluidNCClient::sendCommand(retract_command);
    }
    
    // Restore original distance mode (G90 or G91)
    FluidNCClient::sendCommand(saved_distance_mode);
}

void UITabControlProbe::updateResult(const char* message) {
    if (results_text && message) {
        lv_textarea_set_text(results_text, message);
    }
}

// Textarea focused event handler - show keyboard
static void textarea_focused_event_handler(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    UITabControlProbe::showKeyboard(ta);
}

// Show keyboard
void UITabControlProbe::showKeyboard(lv_obj_t *ta) {
    if (!keyboard) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(keyboard, SCREEN_WIDTH, 220);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);  // Numeric keyboard for probe parameters
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabControlProbe::hideKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabControlProbe::hideKeyboard(); }, LV_EVENT_CANCEL, nullptr);
        if (parent_tab) {
            lv_obj_add_event_cb(parent_tab, [](lv_event_t *e) { UITabControlProbe::hideKeyboard(); }, LV_EVENT_CLICKED, nullptr);
        }
    }
    
    // Enable scrolling on parent tab and add extra padding at bottom for keyboard (every time keyboard is shown)
    if (parent_tab) {
        lv_obj_add_flag(parent_tab, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_bottom(parent_tab, 240, 0); // Extra space for scrolling (keyboard height + margin)
    }
    
    lv_keyboard_set_textarea(keyboard, ta);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Scroll the parent tab to position the focused textarea just above keyboard
    if (parent_tab && ta) {
        // Get textarea position within parent_tab
        lv_coord_t ta_y = lv_obj_get_y(ta);
        lv_obj_t *parent = lv_obj_get_parent(ta);
        
        // Walk up parent hierarchy to get cumulative Y position
        while (parent && parent != parent_tab) {
            ta_y += lv_obj_get_y(parent);
            parent = lv_obj_get_parent(parent);
        }
        
        // Calculate scroll position to place textarea just above keyboard
        // Status bar is 60px, keyboard is 220px, so visible area is 200px (480 - 60 - 220)
        lv_coord_t visible_height = 200; // Height above keyboard and below status bar
        lv_coord_t ta_height = lv_obj_get_height(ta);
        lv_coord_t target_position = visible_height - ta_height - 10; // 10px margin above keyboard
        
        // Scroll amount = (textarea Y position) - (where we want it)
        lv_coord_t scroll_y = ta_y - target_position;
        if (scroll_y < 0) scroll_y = 0; // Don't scroll past top
        
        lv_obj_scroll_to_y(parent_tab, scroll_y, LV_ANIM_ON);
    }
}

// Hide keyboard
void UITabControlProbe::hideKeyboard() {
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        
        // Restore parent tab to non-scrollable and remove extra padding
        if (parent_tab) {
            lv_obj_clear_flag(parent_tab, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_pad_bottom(parent_tab, 10, 0); // Back to original padding
            lv_obj_scroll_to_y(parent_tab, 0, LV_ANIM_ON); // Reset scroll position
        }
    }
}
