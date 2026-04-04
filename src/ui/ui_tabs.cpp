#include "ui/ui_tabs.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "ui/tabs/ui_tab_status.h"
#include "ui/tabs/ui_tab_control.h"
#include "ui/tabs/ui_tab_files.h"
#include "ui/tabs/ui_tab_macros.h"
#include "ui/tabs/ui_tab_terminal.h"
#include "ui/tabs/ui_tab_settings.h"
#include "core/usb_host_manager.h"
#include "core/qtdial_hid_protocol.h"
#include "core/comm_manager.h"

// Static member initialization
lv_obj_t *UITabs::tabview = nullptr;
lv_obj_t *UITabs::tab_status = nullptr;
lv_obj_t *UITabs::tab_control = nullptr;
lv_obj_t *UITabs::tab_files = nullptr;
lv_obj_t *UITabs::tab_macros = nullptr;
lv_obj_t *UITabs::tab_terminal = nullptr;
lv_obj_t *UITabs::tab_settings = nullptr;
lv_obj_t *UITabs::keyboard_toggle_btn = nullptr;
lv_obj_t *UITabs::keyboard_toggle_label = nullptr;
lv_timer_t *UITabs::macros_record_indicator_timer = nullptr;

namespace {
void sync_hid_axis_screens(uint32_t active_tab);
void keyboard_toggle_event_cb(lv_event_t *e);
void macros_record_indicator_timer_cb(lv_timer_t *timer);
} // namespace

// Create main tabview and all tabs
void UITabs::createTabs() {
    Serial.println("UITabs: createTabs start");
    // Create tabview
    tabview = lv_tabview_create(lv_screen_active());
    lv_obj_set_size(tabview, SCREEN_WIDTH, SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
    lv_obj_align(tabview, LV_ALIGN_TOP_LEFT, 0, 0);  // Align to top-left, not bottom
    
    // Get the tab bar and set height
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_height(tab_bar, TAB_BUTTON_HEIGHT);
    const lv_coord_t keyboard_toggle_w = UI_SCALE_X(54);
    const lv_coord_t keyboard_toggle_gap = UI_SCALE_X(6);
    //lv_obj_set_width(tab_bar, SCREEN_WIDTH - keyboard_toggle_w - keyboard_toggle_gap);
    
    // Remove padding from tabview content area
    lv_obj_set_style_pad_all(lv_tabview_get_content(tabview), 0, 0);
    
    // Disable scrolling on tab buttons and content
    lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);
    
    // Style tab buttons with slightly tighter spacing to leave room for the keyboard toggle.
    lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_18, 0);  // Direct to tab bar
    lv_obj_set_style_text_font(tabview, &lv_font_montserrat_18, LV_PART_ITEMS);
    lv_obj_set_style_pad_hor(tabview, UI_SCALE_X(8), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tabview, UITheme::BG_BUTTON, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tabview, UITheme::TEXT_LIGHT, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tabview, UITheme::ACCENT_PRIMARY, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tabview, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);

    // Add tabs
    tab_status = lv_tabview_add_tab(tabview, "Status");
    tab_control = lv_tabview_add_tab(tabview, "Control");
    tab_files = lv_tabview_add_tab(tabview, "Files");
    tab_macros = lv_tabview_add_tab(tabview, "Macros");
    tab_terminal = lv_tabview_add_tab(tabview, "Terminal");
    tab_settings = lv_tabview_add_tab(tabview, "Settings");
    
    // Disable scrolling only on tabs that don't need it
    lv_obj_clear_flag(tab_status, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_control, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_macros, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_files, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_terminal, LV_OBJ_FLAG_SCROLLABLE);
    // Settings tab may need scrolling, so leave it enabled
    

    keyboard_toggle_btn = lv_button_create(tab_bar);
    lv_obj_set_size(keyboard_toggle_btn, keyboard_toggle_w, TAB_BUTTON_HEIGHT - UI_SCALE_Y(10));
    lv_obj_align(keyboard_toggle_btn, LV_ALIGN_RIGHT_MID, -UI_SCALE_X(4), 0);
    lv_obj_add_event_cb(keyboard_toggle_btn, keyboard_toggle_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_radius(keyboard_toggle_btn, UI_SCALE_Y(8), 0);

    keyboard_toggle_label = lv_label_create(keyboard_toggle_btn);
    lv_label_set_text(keyboard_toggle_label, "KB");
    lv_obj_set_style_text_font(keyboard_toggle_label, &lv_font_montserrat_16, 0);
    lv_obj_center(keyboard_toggle_label);
    updateKeyboardToggleButton();

    // Create tab content
    createStatusTab(tab_status);
    createControlTab(tab_control);
    createFilesTab(tab_files);
    createMacrosTab(tab_macros);
    createTerminalTab(tab_terminal);
    createSettingsTab(tab_settings);
    
    // Add event handler for tab changes
    lv_obj_add_event_cb(tabview, tab_changed_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    if (!macros_record_indicator_timer) {
        macros_record_indicator_timer = lv_timer_create(macros_record_indicator_timer_cb, 250, nullptr);
    }

    sync_hid_axis_screens(0);
    updateMacrosTabRecordingIndicator();
    Serial.println("UITabs: createTabs done");
}

namespace {
void keyboard_toggle_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    UICommon::toggleOnScreenKeyboardEnabled();
}

void sync_hid_axis_screens(uint32_t active_tab)
{
    (void)active_tab;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
    UsbHostManager::sendDisplaySelectScreenForRole(QtdialHidProtocol::kRoleX, "cnc_axis", false);
    UsbHostManager::sendDisplaySelectScreenForRole(QtdialHidProtocol::kRoleY, "cnc_axis", false);
    UsbHostManager::sendDisplaySelectScreenForRole(QtdialHidProtocol::kRoleZ, "cnc_axis", false);
#endif
}

void macros_record_indicator_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    UITabs::updateMacrosTabRecordingIndicator();
}
} // namespace

void UITabs::updateKeyboardToggleButton()
{
    if (!keyboard_toggle_btn || !keyboard_toggle_label) {
        return;
    }

    const bool enabled = UICommon::isOnScreenKeyboardEnabled();
    lv_obj_set_style_bg_color(keyboard_toggle_btn,
                              enabled ? UITheme::ACCENT_PRIMARY : UITheme::BG_BUTTON,
                              0);
    lv_obj_set_style_text_color(keyboard_toggle_btn,
                                enabled ? lv_color_white() : UITheme::TEXT_MEDIUM,
                                0);
    lv_obj_set_style_text_color(keyboard_toggle_label,
                                enabled ? lv_color_white() : UITheme::TEXT_MEDIUM,
                                0);
}

void UITabs::updateMacrosTabRecordingIndicator()
{
    if (!tabview) {
        return;
    }

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    if (!tab_bar) {
        return;
    }

    lv_obj_t *macros_tab_btn = lv_obj_get_child(tab_bar, 3);
    if (!macros_tab_btn) {
        return;
    }
    const bool is_selected = getActiveTab() == 3;
    const lv_color_t normal_bg = lv_obj_get_style_bg_color(tabview, LV_PART_ITEMS | LV_STATE_DEFAULT);
    const lv_color_t selected_bg = lv_obj_get_style_bg_color(tabview, LV_PART_ITEMS | LV_STATE_CHECKED);
    const lv_opa_t normal_bg_opa = lv_obj_get_style_bg_opa(tabview, LV_PART_ITEMS | LV_STATE_DEFAULT);
    const lv_opa_t selected_bg_opa = lv_obj_get_style_bg_opa(tabview, LV_PART_ITEMS | LV_STATE_CHECKED);

    if (!UITabMacros::isRecording()) {
        lv_obj_set_style_bg_color(macros_tab_btn,
                                  is_selected ? selected_bg : normal_bg,
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(macros_tab_btn,
                                is_selected ? selected_bg_opa : normal_bg_opa,
                                LV_PART_MAIN);
        return;
    }

    const uint32_t phase_ms = millis() % 1600U;
    const bool pulse_on = phase_ms < 800U;
    const lv_color_t bg_color = pulse_on
        ? UITheme::STATE_NOTICE
        : (is_selected ? selected_bg : normal_bg);
    const lv_opa_t bg_opa = pulse_on ? LV_OPA_COVER : (is_selected ? selected_bg_opa : normal_bg_opa);

    lv_obj_set_style_bg_color(macros_tab_btn, bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(macros_tab_btn, bg_opa, LV_PART_MAIN);
}

// Tab change event handler
void UITabs::tab_changed_event_cb(lv_event_t *e) {
    lv_obj_t *tabview = (lv_obj_t*)lv_event_get_target(e);
    uint32_t active_tab = lv_tabview_get_tab_active(tabview);
    updateKeyboardToggleButton();
    
    // Tab indices: 0=Status, 1=Control, 2=Files, 3=Macros, 4=Terminal, 5=Settings
    if (active_tab == 2) {  // Files tab
        // Trigger initial load on first selection
        UITabFiles::refreshFileList();
    }

    sync_hid_axis_screens(active_tab);
}

// Create Status tab content (delegated to UITabStatus module)
void UITabs::createStatusTab(lv_obj_t *tab) {
    UITabStatus::create(tab);
}

// Create Control tab content (delegated to UITabControl module)
void UITabs::createControlTab(lv_obj_t *tab) {
    UITabControl::create(tab);
}

// Create Files tab content (delegated to UITabFiles module)
void UITabs::createFilesTab(lv_obj_t *tab) {
    UITabFiles::create(tab);
}

// Create Macros tab content (delegated to UITabMacros module)
void UITabs::createMacrosTab(lv_obj_t *tab) {
    UITabMacros::create(tab);
}

// Create Terminal tab content (delegated to UITabTerminal module)
void UITabs::createTerminalTab(lv_obj_t *tab) {
    UITabTerminal::create(tab);

    // Register terminal callback to receive CNC messages (excluding status reports)
    CommManager::setTerminalCallback([](const char* message) {
        UITabTerminal::appendMessage(message);
    });
}

// Create Settings tab content (delegated to UITabSettings module)
void UITabs::createSettingsTab(lv_obj_t *tab) {
    UITabSettings::create(tab);
}

void UITabs::setActiveTab(uint32_t index, lv_anim_enable_t anim) {
    if (!tabview) {
        return;
    }
    lv_tabview_set_active(tabview, index, anim);
}

uint32_t UITabs::getActiveTab() {
    if (!tabview) {
        return 0;
    }
    return lv_tabview_get_tab_active(tabview);
}

uint32_t UITabs::getTabCount() {
    if (!tabview) {
        return 0;
    }
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    return tab_bar ? lv_obj_get_child_count(tab_bar) : 0;
}

void UITabs::cycleTabs() {
    const uint32_t count = getTabCount();
    if (count == 0) {
        return;
    }
    const uint32_t next = (getActiveTab() + 1) % count;
    setActiveTab(next, LV_ANIM_OFF);
}

// Public wrappers for settings functions
void UITabs::loadSettings() {
    // Settings are now loaded by the individual subtabs (General and Connection)
}

void UITabs::saveSettings() {
    // Settings are now saved by the individual subtabs (General and Connection)
}
