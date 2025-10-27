#include "ui/tabs/settings/ui_tab_settings_about.h"
#include "ui/ui_theme.h"
#include "config.h"

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
    lv_label_set_text(version, "Version: v0.01_PRE-ALPHA");
    lv_obj_set_style_text_font(version, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(version, UITheme::TEXT_MEDIUM, 0);
    
    // GitHub section
    lv_obj_t *github_title = lv_label_create(container);
    lv_label_set_text(github_title, "GitHub");
    lv_obj_set_style_text_font(github_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(github_title, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_style_pad_top(github_title, 10, 0);
    
    lv_obj_t *github_link = lv_label_create(container);
    lv_label_set_text(github_link, "github.com/jeyeager65/FluidTouch");
    lv_obj_set_style_text_font(github_link, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(github_link, UITheme::UI_INFO, 0);
    
    // Libraries section
    lv_obj_t *libraries_title = lv_label_create(container);
    lv_label_set_text(libraries_title, "Libraries");
    lv_obj_set_style_text_font(libraries_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(libraries_title, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_style_pad_top(libraries_title, 10, 0);
    
    lv_obj_t *libraries_desc = lv_label_create(container);
    lv_label_set_text(libraries_desc, 
        "LVGL 9.4.0\n"
        "LovyanGFX 1.2.7\n"
        "WebSockets 2.7.1\n"
        "ArduinoJson 7.4.2");
    lv_obj_set_style_text_font(libraries_desc, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(libraries_desc, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(libraries_desc, LV_TEXT_ALIGN_CENTER, 0);
}
