#include "ui/tabs/ui_tab_status.h"
#include "ui/ui_theme.h"

void UITabStatus::create(lv_obj_t *tab) {
    // Industrial Electronics Style Status Display
    lv_obj_set_style_bg_color(tab, UITheme::BG_BLACK, LV_PART_MAIN);
    
    // MACHINE STATE - Industrial header
    lv_obj_t *state_label = lv_label_create(tab);
    lv_label_set_text(state_label, "STATE");
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(state_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(state_label, 15, 5);

    lv_obj_t *state_value = lv_label_create(tab);
    lv_label_set_text(state_value, "IDLE");
    lv_obj_set_style_text_font(state_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(state_value, UITheme::STATE_IDLE, 0);
    lv_obj_set_pos(state_value, 15, 25);

    // Separator line
    lv_obj_t *line1 = lv_obj_create(tab);
    lv_obj_set_size(line1, 770, 2);
    lv_obj_set_pos(line1, 15, 70);
    lv_obj_set_style_bg_color(line1, UITheme::BG_BUTTON, 0);
    lv_obj_set_style_border_width(line1, 0, 0);

    // WORK POSITION - Left column
    lv_obj_t *wpos_header = lv_label_create(tab);
    lv_label_set_text(wpos_header, "WORK POSITION");
    lv_obj_set_style_text_font(wpos_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wpos_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(wpos_header, 15, 80);

    lv_obj_t *wpos_x = lv_label_create(tab);
    lv_label_set_text(wpos_x, "X  0.000");
    lv_obj_set_style_text_font(wpos_x, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(wpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_pos(wpos_x, 15, 105);

    lv_obj_t *wpos_y = lv_label_create(tab);
    lv_label_set_text(wpos_y, "Y  0.000");
    lv_obj_set_style_text_font(wpos_y, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(wpos_y, UITheme::AXIS_Y, 0);
    lv_obj_set_pos(wpos_y, 15, 150);

    lv_obj_t *wpos_z = lv_label_create(tab);
    lv_label_set_text(wpos_z, "Z  0.000");
    lv_obj_set_style_text_font(wpos_z, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(wpos_z, UITheme::AXIS_Z, 0);
    lv_obj_set_pos(wpos_z, 15, 195);

    // MACHINE POSITION - Center column
    lv_obj_t *mpos_header = lv_label_create(tab);
    lv_label_set_text(mpos_header, "MACHINE POSITION");
    lv_obj_set_style_text_font(mpos_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(mpos_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(mpos_header, 220, 80);

    lv_obj_t *mpos_x = lv_label_create(tab);
    lv_label_set_text(mpos_x, "X  0.000");
    lv_obj_set_style_text_font(mpos_x, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(mpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_pos(mpos_x, 220, 105);

    lv_obj_t *mpos_y = lv_label_create(tab);
    lv_label_set_text(mpos_y, "Y  0.000");
    lv_obj_set_style_text_font(mpos_y, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(mpos_y, UITheme::AXIS_Y, 0);
    lv_obj_set_pos(mpos_y, 220, 150);

    lv_obj_t *mpos_z = lv_label_create(tab);
    lv_label_set_text(mpos_z, "Z  0.000");
    lv_obj_set_style_text_font(mpos_z, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(mpos_z, UITheme::AXIS_Z, 0);
    lv_obj_set_pos(mpos_z, 220, 195);

    // MODAL STATES - Right column
    lv_obj_t *modal_header = lv_label_create(tab);
    lv_label_set_text(modal_header, "MODAL");
    lv_obj_set_style_text_font(modal_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(modal_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(modal_header, 500, 80);

    lv_obj_t *status_coord = lv_label_create(tab);
    lv_label_set_text(status_coord, "WCS   G54");
    lv_obj_set_style_text_font(status_coord, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(status_coord, UITheme::POS_MODAL, 0);
    lv_obj_set_pos(status_coord, 500, 105);

    lv_obj_t *status_plane = lv_label_create(tab);
    lv_label_set_text(status_plane, "PLANE G17");
    lv_obj_set_style_text_font(status_plane, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(status_plane, UITheme::UI_SECONDARY, 0);
    lv_obj_set_pos(status_plane, 500, 140);

    lv_obj_t *status_dist = lv_label_create(tab);
    lv_label_set_text(status_dist, "DIST  G90");
    lv_obj_set_style_text_font(status_dist, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(status_dist, UITheme::UI_SECONDARY, 0);
    lv_obj_set_pos(status_dist, 500, 175);

    lv_obj_t *status_units = lv_label_create(tab);
    lv_label_set_text(status_units, "UNITS G21");
    lv_obj_set_style_text_font(status_units, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(status_units, UITheme::POS_WORK, 0);
    lv_obj_set_pos(status_units, 500, 210);

    // FEED RATE & SPINDLE - Bottom row
    lv_obj_t *status_feed_header = lv_label_create(tab);
    lv_label_set_text(status_feed_header, "FEED RATE");
    lv_obj_set_style_text_font(status_feed_header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_feed_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(status_feed_header, 15, 250);

    lv_obj_t *status_feed_value = lv_label_create(tab);
    lv_label_set_text(status_feed_value, "0 mm/min");
    lv_obj_set_style_text_font(status_feed_value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(status_feed_value, lv_color_white(), 0);
    lv_obj_set_pos(status_feed_value, 15, 270);

    lv_obj_t *status_speed_header = lv_label_create(tab);
    lv_label_set_text(status_speed_header, "SPINDLE");
    lv_obj_set_style_text_font(status_speed_header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_speed_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(status_speed_header, 250, 250);

    lv_obj_t *status_speed_value = lv_label_create(tab);
    lv_label_set_text(status_speed_value, "0 RPM");
    lv_obj_set_style_text_font(status_speed_value, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(status_speed_value, lv_color_white(), 0);
    lv_obj_set_pos(status_speed_value, 250, 270);

    lv_obj_t *status_override_header = lv_label_create(tab);
    lv_label_set_text(status_override_header, "OVERRIDE");
    lv_obj_set_style_text_font(status_override_header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_override_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(status_override_header, 500, 250);

    lv_obj_t *status_override_value = lv_label_create(tab);
    lv_label_set_text(status_override_value, "F:100% S:100%");
    lv_obj_set_style_text_font(status_override_value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(status_override_value, lv_color_white(), 0);
    lv_obj_set_pos(status_override_value, 470, 270);
}
