#include "CastPage.h"
#include "../Widgets.h"

namespace ui { namespace cast {
namespace {
lv_obj_t* cards[2];
lv_obj_t* message;
unsigned selected = 0;
void refresh() { focus(cards[0], selected == 0); focus(cards[1], selected == 1); }
}
lv_obj_t* create() {
    auto* root = pageRoot();
    header(root, "投屏", "COMING SOON");
    const char* names[] = {"连接 Wi-Fi", "设备热点"};
    const char* hints[] = {"Wi-Fi CLIENT", "DIRECT AP"};
    constexpr unsigned cardWidth = (viewport::ContentWidth - viewport::Gap) / 2;
    for (unsigned i = 0; i < 2; ++i) {
        cards[i] = panel(root, 12 + i * (cardWidth + viewport::Gap), 38, cardWidth, 78);
        label(cards[i], i ? LV_SYMBOL_UPLOAD : LV_SYMBOL_WIFI, 12, 12,
              i ? 0xBEAAFF : Accent, &lv_font_montserrat_20);
        label(cards[i], names[i], 12, 35);
        label(cards[i], hints[i], 12, 61, Muted, &lv_font_montserrat_12);
    }
    message = label(root, "功能开发中", 0, 126, Muted);
    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 126);
    selected = 0; refresh();
    footer(root, "L/R select  MID details  HOLD back");
    return root;
}
void input(Input event) {
    if (event == Input::Back) { navigate(Page::Home); return; }
    if (event == Input::Left || event == Input::Right) { selected ^= 1; refresh(); }
    if (event == Input::Confirm) {
        lv_label_set_text(message, selected ? "热点功能待开放" : "网络功能待开放");
        lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 126);
    }
}
} }
