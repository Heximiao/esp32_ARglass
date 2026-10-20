#include "AlbumPage.h"
#include "../PhotoStore.h"
#include "../Widgets.h"

namespace ui { namespace album {
namespace {
lv_obj_t* root;
size_t selected = 0;
bool viewer = false;
void render() {
    lv_obj_clean(root);
    lv_obj_set_style_bg_color(root, lv_color_hex(viewer ? 0x000000 : Background), 0);
    char status[32];
    snprintf(status, sizeof(status), "%u / %u", unsigned(photos().count() ? selected + 1 : 0), unsigned(photos().count()));
    if (viewer) {
        const auto* photo = photos().at(selected);
        auto* image = lv_img_create(root);
        imageFit(image, &photo->image, viewport::Width, viewport::Height);
        auto* top = panel(root, 8, 6, 68, 24, Background);
        label(top, status, 7, 5, White, &lv_font_montserrat_12);
        auto* bottom = panel(root, 8, 141, viewport::Width - 16, 26, Background);
        auto* name = label(bottom, photo->name.c_str(), 7, 1);
        lv_obj_set_width(name, viewport::Width - 148);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        auto* controls = label(bottom, "L/R next  MID grid", 0, 7, Muted, &lv_font_montserrat_12);
        lv_obj_align(controls, LV_ALIGN_TOP_RIGHT, -7, 7);
        return;
    }
    header(root, "相册", status);
    if (!photos().count()) {
        auto* box = panel(root, 12, 41, viewport::ContentWidth, 102);
        label(box, LV_SYMBOL_IMAGE, 16, 16, Accent, &lv_font_montserrat_28);
        label(box, "暂无图片", 61, 21);
        label(box, photos().status(), 16, 61, Muted, &lv_font_montserrat_12);
        footer(root, "Upload .rgb565  HOLD back");
        return;
    }
    const size_t first = selected / 4 * 4;
    constexpr unsigned tileWidth = (viewport::ContentWidth - viewport::Gap) / 2;
    for (size_t slot = 0; slot < 4; ++slot) {
        const auto* photo = photos().at(first + slot);
        auto* tile = panel(root, 12 + (slot % 2) * (tileWidth + viewport::Gap),
                           36 + (slot / 2) * 56, tileWidth, 50);
        if (!photo) {
            auto* empty = label(tile, "EMPTY", 0, 0, 0x42556B, &lv_font_montserrat_12);
            lv_obj_center(empty);
            continue;
        }
        focus(tile, first + slot == selected);
        auto* preview = panel(tile, 5, 5, 44, 40, Background);
        lv_obj_set_style_border_width(preview, 0, 0);
        lv_obj_set_style_radius(preview, 5, 0);
        imageFit(lv_img_create(preview), &photo->image, 42, 38);
        auto* name = label(tile, photo->name.c_str(), 57, 2);
        lv_obj_set_width(name, tileWidth - 64);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        char dimensions[20];
        snprintf(dimensions, sizeof(dimensions), "%ux%u", photo->image.header.w, photo->image.header.h);
        auto* size = label(tile, dimensions, 57, 28, Muted, &lv_font_montserrat_12);
        lv_obj_set_width(size, tileWidth - 61);
        lv_label_set_long_mode(size, LV_LABEL_LONG_DOT);
    }
    footer(root, "Move select  MID view  HOLD back");
}
}
lv_obj_t* create() {
    root = pageRoot(); selected = 0; viewer = false;
    render(); return root;
}
void input(Input event) {
    if (event == Input::Back) {
        if (viewer) { viewer = false; render(); }
        else navigate(Page::Home);
        return;
    }
    const size_t count = photos().count();
    if (!count) return;
    if (event == Input::Confirm) viewer = !viewer;
    int step = 0;
    if (event == Input::Left) step = -1;
    if (event == Input::Right) step = 1;
    if (event == Input::Up) step = viewer ? -1 : -2;
    if (event == Input::Down) step = viewer ? 1 : 2;
    if (step) selected = (int(selected) + step + int(count) * 2) % count;
    render();
}
} }
