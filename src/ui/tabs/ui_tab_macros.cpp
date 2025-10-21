#include "ui/tabs/ui_tab_macros.h"
#include "ui/ui_theme.h"

void UITabMacros::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);

    // Macro buttons - 4x3 grid (12 buttons total)
    const char* macro_default_labels[] = {
        "Tool\nChange", "Probe\nZ", "Corner\nProbe", "Go to\nOrigin",
        "Park\nPosition", "Spindle\nTest", "Coolant\nOn", "Coolant\nOff",
        "Safe\nHeight", "Material\nSetup", "Pen\nDown", "Pen\nUp"
    };
    
    const lv_color_t macro_colors[] = {
        UITheme::MACRO_COLOR_1, UITheme::MACRO_COLOR_2, UITheme::MACRO_COLOR_4, UITheme::MACRO_COLOR_3,  // Row 1
        UITheme::MACRO_COLOR_6, UITheme::MACRO_COLOR_1, UITheme::MACRO_COLOR_5, UITheme::MACRO_COLOR_7,  // Row 2
        UITheme::MACRO_COLOR_2, UITheme::MACRO_COLOR_6, UITheme::MACRO_COLOR_5, UITheme::MACRO_COLOR_8   // Row 3
    };

    int macro_btn_width = 170;
    int macro_btn_height = 95;
    int macro_start_x = 10;
    int macro_start_y = 10;
    int macro_spacing_x = 185;
    int macro_spacing_y = 110;

    for (int i = 0; i < 12; i++) {
        int row = i / 4;
        int col = i % 4;
        
        lv_obj_t *btn_macro = lv_button_create(tab);
        lv_obj_set_size(btn_macro, macro_btn_width, macro_btn_height);
        lv_obj_set_pos(btn_macro, macro_start_x + col * macro_spacing_x, macro_start_y + row * macro_spacing_y);
        lv_obj_set_style_bg_color(btn_macro, macro_colors[i], LV_PART_MAIN);
        
        lv_obj_t *lbl_macro = lv_label_create(btn_macro);
        lv_label_set_text(lbl_macro, macro_default_labels[i]);
        lv_obj_set_style_text_font(lbl_macro, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_align(lbl_macro, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl_macro);
    }
}
