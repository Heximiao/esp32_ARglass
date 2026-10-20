#pragma once
#include <stdint.h>
namespace joystick { namespace config {
// Passive switch: COM -> GND, pressed inputs read LOW.
// If an axis is reversed, exchange its two pin values here and rebuild.
constexpr uint8_t UpPin = 12;
constexpr uint8_t DownPin = 13;
constexpr uint8_t LeftPin = 14;
constexpr uint8_t RightPin = 15;
constexpr uint8_t MidPin = 16;
constexpr uint32_t DebounceMs=25, LongPressMs=650;
constexpr uint32_t RepeatDelayMs=400, RepeatIntervalMs=150;
} }
