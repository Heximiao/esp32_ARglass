#include "Widgets.h"
#include "fonts/UiFont.h"
#include <algorithm>

namespace ui {
lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 12, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0x25364B), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    return o;
}
lv_obj_t* label(lv_obj_t* parent, const char* text, int x, int y,
                uint32_t color, const lv_font_t* font) {
    lv_obj_t* o = lv_label_create(parent);
    lv_obj_set_style_text_font(o, font ? font : &ui_font_16, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_text(o, text);
    lv_obj_set_pos(o, x, y);
    return o;
}
lv_obj_t* pageRoot() {
    lv_obj_t* o = lv_obj_create(nullptr);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, viewport::Width, viewport::Height);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(o, lv_color_hex(Background), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}
void header(lv_obj_t* root, const char* title, const char* status) {
    label(root, title, 12, 9);
    auto* s = label(root, status, 0, 12, Muted, &lv_font_montserrat_12);
    lv_obj_align(s, LV_ALIGN_TOP_RIGHT, -12, 12);
}
void footer(lv_obj_t* root, const char* text) {
    auto* line = panel(root, 12, 150, viewport::ContentWidth, 1, 0x25364B);
    lv_obj_set_style_border_width(line, 0, 0);
    auto* hint = label(root, text, 12, 156, Muted, &lv_font_montserrat_12);
    lv_obj_set_width(hint, viewport::ContentWidth);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
}
void focus(lv_obj_t* o, bool selected) {
    lv_obj_set_style_border_color(o, lv_color_hex(selected ? Accent : 0x25364B), 0);
    lv_obj_set_style_border_width(o, selected ? 2 : 1, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(selected ? 0x173340 : Surface), 0);
}
void imageFit(lv_obj_t* image, const lv_img_dsc_t* source, int w, int h) {
    lv_img_set_src(image, source);
    const unsigned zoom = std::min(unsigned(w) * 256 / source->header.w,
                                   unsigned(h) * 256 / source->header.h);
    lv_img_set_zoom(image, zoom);
    lv_img_set_antialias(image, true);
    lv_obj_center(image);
}
}
