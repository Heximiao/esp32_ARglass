#pragma once
#include "UiTypes.h"
namespace ui {
void begin();
void tick();
void input(Input event); // Future joystick adapter calls this on the UI task.
}
