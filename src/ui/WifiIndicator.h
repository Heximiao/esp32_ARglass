#pragma once
#include <lvgl.h>
namespace ui { namespace wifiindicator {
void create(lv_obj_t* parent);
void tick(); // Call only while the owning page is alive.
} }
