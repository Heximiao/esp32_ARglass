#pragma once
#include <stdint.h>
namespace joystick {
void begin();
// Poll on the UI task: no blocking delays or interrupt handlers.
void tick();
// Debounced active switches: bits 0..4 = UP, DOWN, LEFT, RIGHT, MID.
uint8_t pressedMask();
}
