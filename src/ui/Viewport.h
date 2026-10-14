#pragma once
#include "../spi/St7789.h"

namespace ui { namespace viewport {
// Reflow the UI into a narrower optical safe area; pixels and fonts stay native size.
constexpr unsigned WidthPercent = 80;
static_assert(WidthPercent >= 75 && WidthPercent <= 100, "UI width must be 75..100 percent");
constexpr unsigned Width = (St7789::Width * WidthPercent + 50) / 100;
constexpr unsigned Height = St7789::Height;
constexpr unsigned Left = (St7789::Width - Width) / 2;
constexpr unsigned Top = (St7789::Height - Height) / 2;
constexpr unsigned Margin = 12;
constexpr unsigned Gap = 8;
constexpr unsigned ContentWidth = Width - 2 * Margin;
} }
