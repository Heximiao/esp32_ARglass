#pragma once
#include <lvgl.h>

namespace ui {
enum class Page { Home, Album, Animation, Cast, Settings, KeyDebug };
enum class Input { Up, Down, Left, Right, Confirm, Back };
struct Preferences {
    unsigned brightness = 75;
    unsigned speed = 1; // slow / normal / fast
    bool animationInfo = true;
};
void navigate(Page page);
Preferences& preferences();
}
