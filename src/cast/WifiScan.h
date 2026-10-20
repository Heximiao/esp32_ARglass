#pragma once
#include <Arduino.h>
namespace casting { namespace networks {
struct Network { String ssid; int rssi; bool open; bool supported; };
bool scan();
void tick();
bool scanning();
bool failed();
unsigned count();
const Network& at(unsigned index);
} }
