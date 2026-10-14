#include "AnimationPage.h"
#include "../PhotoStore.h"
#include "../Widgets.h"
#include <algorithm>

namespace ui { namespace animation {
namespace {
lv_obj_t* root;
lv_obj_t* moving = nullptr;
lv_obj_t* state = nullptr;
size_t selected = 0;
bool paused = false;
float x = 10, y = 8, vx = 43, vy = 61;
int imageWidth = 0, imageHeight = 0, top = 0, areaHeight = 172;
uint32_t previous = 0;
void reflect(float& p, float& velocity, float limit, float dt) {
    if (limit <= 0) { p = 0; return; }
    p += velocity * dt;
    while (p < 0 || p > limit) {
        if (p < 0) { p = -p; velocity = -velocity; }
        if (p > limit) { p = 2 * limit - p; velocity = -velocity; }
    }
}
void render() {
    lv_obj_clean(root); moving = nullptr; state = nullptr;
    const auto* photo = photos().at(selected);
    if (!photo) {
        header(root, "图片动画", "NO PHOTOS");
        auto* empty = label(root, "暂无图片", 0, 0);
        lv_obj_center(empty);
        footer(root, "Upload photos   B home");
        return;
    }
    const bool info = preferences().animationInfo;
    top = info ? 36 : 0;
    areaHeight = info ? 111 : viewport::Height;
    if (info) {
        label(root, "图片动画", 12, 9);
        state = label(root, paused ? "PAUSED" : "PLAYING", 0, 12, Accent, &lv_font_montserrat_12);
        lv_obj_align(state, LV_ALIGN_TOP_RIGHT, -12, 12);
        footer(root, "E pause  A/D photo  S speed  W info");
    }
    const unsigned zoom = std::min(256U, std::min(112U * 256 / photo->image.header.w, 100U * 256 / photo->image.header.h));
    imageWidth = std::max(1U, photo->image.header.w * zoom / 256);
    imageHeight = std::max(1U, photo->image.header.h * zoom / 256);
    moving = panel(root, 10, top + 8, imageWidth, imageHeight, 0x000000);
    lv_obj_set_style_border_width(moving, 0, 0);
    lv_obj_set_style_radius(moving, 0, 0);
    auto* image = lv_img_create(moving);
    lv_img_set_src(image, &photo->image);
    lv_img_set_zoom(image, zoom);
    lv_obj_center(image);
    x = 10; y = 8; vx = 43; vy = 61;
    previous = millis();
}
}
lv_obj_t* create() {
    root = pageRoot(); selected = 0; paused = false;
    lv_obj_set_style_bg_color(root, lv_color_hex(0x000000), 0);
    render(); return root;
}
void input(Input event) {
    if (event == Input::Back) { navigate(Page::Home); return; }
    if (event == Input::Confirm) {
        paused = !paused;
        if (state) lv_label_set_text(state, paused ? "PAUSED" : "PLAYING");
    }
    if ((event == Input::Left || event == Input::Right) && photos().count()) {
        const int count = photos().count();
        selected = (int(selected) + count + (event == Input::Left ? -1 : 1)) % count;
        render();
    }
    if (event == Input::Down) {
        preferences().speed = (preferences().speed + 1) % 3;
        Serial.printf("Animation speed: %u\n", preferences().speed);
    }
    if (event == Input::Up) {
        preferences().animationInfo = !preferences().animationInfo;
        render();
    }
}
void tick() {
    const uint32_t now = millis();
    const uint32_t elapsed = now - previous;
    if (elapsed < 33) return;
    previous = now;
    if (!moving || paused) return;
    const float speeds[] = {0.5f, 1.0f, 1.7f};
    const float dt = std::min(elapsed, uint32_t(100)) / 1000.0f * speeds[preferences().speed];
    reflect(x, vx, viewport::Width - imageWidth, dt);
    reflect(y, vy, areaHeight - imageHeight, dt);
    lv_obj_set_pos(moving, int(x), top + int(y));
}
} }
