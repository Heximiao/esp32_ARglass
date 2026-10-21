#include "HomePage.h"
#include "../Widgets.h"
#include "../WifiIndicator.h"

namespace ui { namespace home {
namespace {
lv_obj_t* cards[4];
lv_obj_t* strip;
lv_obj_t* counter;
unsigned selected = 0;
unsigned firstVisible = 0;
constexpr unsigned CardWidth = (viewport::ContentWidth - 2 * viewport::Gap) / 3;
void refresh(lv_anim_enable_t animation = LV_ANIM_ON) {
    for (unsigned i = 0; i < 4; ++i) focus(cards[i], i == selected);
    if (selected < firstVisible) firstVisible = selected;
    if (selected >= firstVisible + 3) firstVisible = selected - 2;
    lv_label_set_text_fmt(counter, "%02u / 04", selected + 1);
    lv_obj_update_layout(strip);
    lv_obj_scroll_to_x(strip, firstVisible * (CardWidth + viewport::Gap), animation);
}
}
lv_obj_t* create() {
    auto* root = pageRoot();
    label(root, "AR / GLASS", 12, 11, White, &lv_font_montserrat_14);
    counter = label(root, "", 0, 12, Accent, &lv_font_montserrat_12);
    lv_obj_align(counter, LV_ALIGN_TOP_RIGHT, -12, 12);
    wifiindicator::create(root);
    strip = scrollArea(root, 12, 43, viewport::ContentWidth, 100, LV_DIR_HOR);
    const char* names[] = {"相册", "动画", "投屏", "设置"};
    const char* notes[] = {"PHOTOS", "BOUNCE", "CAST", "SYSTEM"};
    const char* icons[] = {LV_SYMBOL_IMAGE, LV_SYMBOL_PLAY, LV_SYMBOL_WIFI, LV_SYMBOL_SETTINGS};
    const uint32_t colors[] = {0x53DDDF, 0xBEAAFF, 0xFFC68B, 0x9DE2AD};
    for (unsigned i = 0; i < 4; ++i) {
        cards[i] = panel(strip, i * (CardWidth + viewport::Gap), 0, CardWidth, 100);
        auto* badge = panel(cards[i], 10, 10, 34, 32, 0x203348);
        lv_obj_set_style_border_width(badge, 0, 0);
        auto* icon = label(badge, icons[i], 0, 0, colors[i], &lv_font_montserrat_20);
        lv_obj_center(icon);
        label(cards[i], names[i], 10, 53);
        label(cards[i], notes[i], 8, 79, Muted, &lv_font_montserrat_12);
    }
    refresh(LV_ANIM_OFF);
    footer(root, "L/R scroll  MID open  HOLD back");
    return root;
}
void tick(){wifiindicator::tick();}
void input(Input event) {
    if (event == Input::Left || event == Input::Up) selected = (selected + 3) % 4;
    if (event == Input::Right || event == Input::Down) selected = (selected + 1) % 4;
    if (event == Input::Confirm) {
        const Page destinations[] = {Page::Album, Page::Animation, Page::Cast, Page::Settings};
        navigate(destinations[selected]);
        return;
    }
    refresh();
}
} }
