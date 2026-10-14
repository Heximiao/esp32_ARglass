#include "HomePage.h"
#include "../Widgets.h"

namespace ui { namespace home {
namespace {
lv_obj_t* cards[4];
unsigned selected = 0;
void refresh() { for (unsigned i = 0; i < 4; ++i) focus(cards[i], i == selected); }
}
lv_obj_t* create() {
    auto* root = pageRoot();
    label(root, "AR / GLASS", 12, 11, White, &lv_font_montserrat_14);
    auto* status = label(root, "OFFLINE", 0, 12, Muted, &lv_font_montserrat_12);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -76, 12);
    cards[3] = panel(root, viewport::Width - 60, 6, 48, 28);
    auto* gear = label(cards[3], LV_SYMBOL_SETTINGS, 0, 0, Muted, &lv_font_montserrat_14);
    lv_obj_center(gear);
    const char* names[] = {"相册", "动画", "投屏"};
    const char* notes[] = {"PHOTOS", "BOUNCE", "SOON"};
    const char* icons[] = {LV_SYMBOL_IMAGE, LV_SYMBOL_PLAY, LV_SYMBOL_WIFI};
    const uint32_t colors[] = {0x53DDDF, 0xBEAAFF, 0xFFC68B};
    constexpr unsigned cardWidth = (viewport::ContentWidth - 2 * viewport::Gap) / 3;
    for (unsigned i = 0; i < 3; ++i) {
        cards[i] = panel(root, 12 + i * (cardWidth + viewport::Gap), 43, cardWidth, 100);
        auto* badge = panel(cards[i], 10, 10, 34, 32, 0x203348);
        lv_obj_set_style_border_width(badge, 0, 0);
        auto* icon = label(badge, icons[i], 0, 0, colors[i], &lv_font_montserrat_20);
        lv_obj_center(icon);
        label(cards[i], names[i], 10, 53);
        label(cards[i], notes[i], 8, 79, Muted, &lv_font_montserrat_12);
    }
    selected = 0;
    refresh();
    footer(root, "A/D select  E open  W settings");
    return root;
}
void input(Input event) {
    if (event == Input::Left) selected = selected == 3 ? 2 : (selected + 2) % 3;
    if (event == Input::Right) selected = selected == 3 ? 0 : (selected + 1) % 3;
    if (event == Input::Up) selected = 3;
    if (event == Input::Down && selected == 3) selected = 0;
    if (event == Input::Confirm) {
        const Page destinations[] = {Page::Album, Page::Animation, Page::Cast, Page::Settings};
        navigate(destinations[selected]);
        return;
    }
    refresh();
}
} }
