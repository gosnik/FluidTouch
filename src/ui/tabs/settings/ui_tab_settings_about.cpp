#include "ui/tabs/settings/ui_tab_settings_about.h"
#include "ui/ui_theme.h"
#include "config.h"
#include "network/screenshot_server.h"
#include "ui/machine_config.h"
#if FT_WIFI_ENABLED
#include "network/wifi_manager.h"
#endif

// Static member initialization
lv_obj_t *UITabSettingsAbout::screenshot_link_label = nullptr;
lv_obj_t *UITabSettingsAbout::screenshot_qr = nullptr;
bool UITabSettingsAbout::wifi_url_set = false;

void UITabSettingsAbout::create(lv_obj_t *tab) {
    // Disable scrolling on the tab itself
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create container for content
    lv_obj_t *container = lv_obj_create(tab);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(container, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 20, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(container, 15, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Project name (large, colored)
    lv_obj_t *project_name = lv_label_create(container);
    lv_label_set_text(project_name, "FluidTouch");
    lv_obj_set_style_text_font(project_name, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(project_name, UITheme::ACCENT_PRIMARY, 0);
    
    // Version (medium, gray)
    lv_obj_t *version = lv_label_create(container);
    lv_label_set_text(version, "Version: " FLUIDTOUCH_VERSION);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(version, UITheme::TEXT_MEDIUM, 0);
    
    // Horizontal container for both columns
    lv_obj_t *columns_container = lv_obj_create(container);
    lv_obj_set_size(columns_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(columns_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(columns_container, 0, 0);
    lv_obj_set_style_pad_all(columns_container, 0, 0);
    lv_obj_set_flex_flow(columns_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(columns_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(columns_container, 60, 0);
    lv_obj_set_style_pad_top(columns_container, 20, 0);
    
    // Left column: GitHub
    lv_obj_t *github_column = lv_obj_create(columns_container);
    lv_obj_set_size(github_column, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(github_column, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(github_column, 0, 0);
    lv_obj_set_style_pad_all(github_column, 0, 0);
    lv_obj_set_flex_flow(github_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(github_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(github_column, 10, 0);
    
    // GitHub title
    lv_obj_t *github_title = lv_label_create(github_column);
    lv_label_set_text(github_title, "Documentation & Source Code");
    lv_obj_set_style_text_font(github_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(github_title, UITheme::ACCENT_SECONDARY, 0);
    
    // Right column: Screenshot Server
    lv_obj_t *screenshot_column = lv_obj_create(columns_container);
    lv_obj_set_size(screenshot_column, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(screenshot_column, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(screenshot_column, 0, 0);
    lv_obj_set_style_pad_all(screenshot_column, 0, 0);
    lv_obj_set_flex_flow(screenshot_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screenshot_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(screenshot_column, 10, 0);
    
    // Screenshot title
    lv_obj_t *screenshot_title = lv_label_create(screenshot_column);
    lv_label_set_text(screenshot_title, "Screenshot Server");
    lv_obj_set_style_text_font(screenshot_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(screenshot_title, UITheme::ACCENT_SECONDARY, 0);
    
    // Screenshot link
    screenshot_link_label = lv_label_create(screenshot_column);
    lv_obj_set_style_text_font(screenshot_link_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(screenshot_link_label, LV_TEXT_ALIGN_CENTER, 0);
    
    update();  // Set initial text and QR code
}

void UITabSettingsAbout::update() {
    if (screenshot_link_label == nullptr || screenshot_qr == nullptr) return;
    
    // Only update once when WiFi connects
    if (wifi_url_set) return;
    
    MachineConfig config;
    bool is_wireless = MachineConfigManager::getSelectedMachine(config)
        && (config.connection_type == CONN_WIRELESS);
    if (!is_wireless) {
        lv_label_set_text(screenshot_link_label, "WiFi not\nused");
        lv_obj_set_style_text_color(screenshot_link_label, UITheme::TEXT_DISABLED, 0);
        return;
    }

    #if FT_WIFI_ENABLED
    if (WifiManager::isConnected()) {
        String url = "http://" + WifiManager::getLocalIpString();
        lv_label_set_text(screenshot_link_label, url.c_str());
        lv_obj_set_style_text_color(screenshot_link_label, UITheme::UI_INFO, 0);
        
        wifi_url_set = true;  // Mark as set, don't update again
    } else {
        lv_label_set_text(screenshot_link_label, "WiFi not\nconnected");
        lv_obj_set_style_text_color(screenshot_link_label, UITheme::TEXT_DISABLED, 0);
    }
    #else
    lv_label_set_text(screenshot_link_label, "WiFi disabled");
    lv_obj_set_style_text_color(screenshot_link_label, UITheme::TEXT_DISABLED, 0);
    #endif
}
