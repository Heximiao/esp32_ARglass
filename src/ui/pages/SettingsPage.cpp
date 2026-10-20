#include "SettingsPage.h"
#include "../Widgets.h"

namespace ui { namespace settings {
namespace {
lv_obj_t* rows[4];
lv_obj_t* values[4];
unsigned selected = 0;
void refresh() {
    const char* speeds[] = {"SLOW", "NORMAL", "FAST"};
    lv_label_set_text_fmt(values[0], "%u%%", preferences().brightness);
    lv_label_set_text(values[1], speeds[preferences().speed]);
    lv_label_set_text(values[2], preferences().animationInfo ? "ON" : "OFF");
    for (unsigned i = 0; i < 4; ++i) {
        focus(rows[i], i == selected);
        lv_obj_align(values[i], LV_ALIGN_RIGHT_MID, -12, 0);
    }
}
}
lv_obj_t* create() {
    auto* root = pageRoot();
    header(root, "设置", "UI / V1.0");
    const char* titles[] = {"屏幕亮度", "动画速度", "动画信息", "设备信息"};
    for (unsigned i = 0; i < 4; ++i) {
        rows[i] = panel(root, 12, 35 + i * 28, viewport::ContentWidth, 27);
        lv_obj_set_style_radius(rows[i], 8, 0);
        label(rows[i], titles[i], 12, 0);
        values[i] = label(rows[i], i == 3 ? "S3 / 320x172" : "", 0, 0, Accent, &lv_font_montserrat_12);
    }
    selected = 0; refresh();
    footer(root, "U/D select  L/R change  HOLD back");
    return root;
}
void input(Input event) {
    if (event == Input::Back) { navigate(Page::Home); return; }
    if (event == Input::Up) selected = (selected + 3) % 4;
    if (event == Input::Down) selected = (selected + 1) % 4;
    if (event == Input::Confirm || event == Input::Right || event == Input::Left) {
        if (selected == 0) {
            const int value = int(preferences().brightness) + (event == Input::Left ? -10 : 10);
            preferences().brightness = value < 15 ? 15 : (value > 100 ? 100 : value);
        }
        if (selected == 1) preferences().speed = (preferences().speed + (event == Input::Left ? 2 : 1)) % 3;
        if (selected == 2) preferences().animationInfo = !preferences().animationInfo;
    }
    refresh();
}
} }
