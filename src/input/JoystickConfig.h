#pragma once
#include <stdint.h>
namespace joystick { namespace config {
// Passive switch: COM -> GND, pressed inputs read LOW.
constexpr uint8_t UpPin=12, DownPin=13, LeftPin=14, RightPin=15, MidPin=16;
constexpr uint32_t DebounceMs=25, LongPressMs=650;
constexpr uint32_t RepeatDelayMs=400, RepeatIntervalMs=150;
} }
