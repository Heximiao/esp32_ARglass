#pragma once
#include "../spi/St7789.h"
namespace ui {
bool displayBegin(St7789& screen);
void displayTick();
}
