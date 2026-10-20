#pragma once
#include "UiTypes.h"
namespace ui {
void begin();
void tick();
void input(Input event, bool repeated = false); // Joystick and serial share the UI task.
}
