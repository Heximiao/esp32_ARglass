#pragma once
#include "../UiTypes.h"
namespace ui { namespace keydebug {
lv_obj_t* create();
void input(Input event, bool repeated = false);
void tick();
} }
