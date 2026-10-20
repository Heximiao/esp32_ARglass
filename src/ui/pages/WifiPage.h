#pragma once
#include "../UiTypes.h"
namespace ui { namespace wifi {
lv_obj_t* create();
void tick();
void input(Input event);
void leave();
} }
