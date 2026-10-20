#pragma once
namespace joystick {
void begin();
// Poll on the UI task: no blocking delays or interrupt handlers.
void tick();
}
