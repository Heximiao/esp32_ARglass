#pragma once
#include "UiTypes.h"
#include <Arduino.h>
namespace ui { namespace keyboard {
void create(lv_obj_t* root, const char* ssid);
// True only when the user chooses JOIN. Long MID is handled by the parent.
bool input(Input event);
const String& value();
void clear();
void error(const char* message);
} }
