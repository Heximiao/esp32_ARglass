#include "KeyDebugPage.h"
#include "../Widgets.h"
#include "../../input/Joystick.h"
#include "../../input/JoystickConfig.h"
#include <Arduino.h>
#include <algorithm>

namespace ui { namespace keydebug {
namespace {
constexpr int FieldWidth = viewport::ContentWidth;
constexpr int FieldHeight = 82;
constexpr int DotSize = 10;
constexpr int Inset = 3;
constexpr int Step = 6;
lv_obj_t* dot;
lv_obj_t* status;
lv_obj_t* switches;
int x = 0, y = 0;
uint8_t lastMask = 0xff;
void center() { x = (FieldWidth - DotSize) / 2; y = (FieldHeight - DotSize) / 2; }
void moveDot() {
    x = std::max(Inset, std::min(x, FieldWidth - DotSize - Inset));
    y = std::max(Inset, std::min(y, FieldHeight - DotSize - Inset));
    lv_obj_set_pos(dot, x, y);
}
}
lv_obj_t* create() {
    auto* root = pageRoot();
    header(root, "按键调试", "INPUT TEST");
    auto* field = panel(root, 12, 35, FieldWidth, FieldHeight, 0x050A12);
    // A fixed crosshair makes movement and recentering easy to judge.
    auto* vertical = panel(field, FieldWidth / 2, 3, 1, FieldHeight - 6, 0x223044);
    auto* horizontal = panel(field, 3, FieldHeight / 2, FieldWidth - 6, 1, 0x223044);
    lv_obj_set_style_border_width(vertical, 0, 0);
    lv_obj_set_style_border_width(horizontal, 0, 0);
    dot = panel(field, 0, 0, DotSize, DotSize, 0xFF424F);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    center(); moveDot();
    status = label(root, "READY / MID TO CENTER", 12, 121, Accent, &lv_font_montserrat_12);
    switches = label(root, "", 12, 136, Muted, &lv_font_montserrat_12);
    lastMask = 0xff;
    tick();
    footer(root, "Move dot  MID center  HOLD back");
    return root;
}
void input(Input event, bool repeated) {
    if (event == Input::Back) { navigate(Page::Settings); return; }
    const char* name = nullptr;
    unsigned pin = 0;
    switch (event) {
        case Input::Up: y -= Step; name = "UP"; pin = joystick::config::UpPin; break;
        case Input::Down: y += Step; name = "DOWN"; pin = joystick::config::DownPin; break;
        case Input::Left: x -= Step; name = "LEFT"; pin = joystick::config::LeftPin; break;
        case Input::Right: x += Step; name = "RIGHT"; pin = joystick::config::RightPin; break;
        case Input::Confirm: center(); name = "MID / CENTER"; pin = joystick::config::MidPin; break;
        default: return;
    }
    moveDot();
    lv_label_set_text_fmt(status, "%s  GPIO%u", name, pin);
    if (!repeated) Serial.printf("Joystick test: %s GPIO%u\n", name, pin);
}
void tick() {
    const uint8_t mask = joystick::pressedMask();
    if (mask == lastMask) return;
    lastMask = mask;
    lv_label_set_text_fmt(switches, "U:%u  D:%u  L:%u  R:%u  M:%u",
        unsigned((mask >> 0) & 1), unsigned((mask >> 1) & 1), unsigned((mask >> 2) & 1),
        unsigned((mask >> 3) & 1), unsigned((mask >> 4) & 1));
}
} }
