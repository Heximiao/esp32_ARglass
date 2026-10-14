#pragma once
#include "UiTypes.h"
#include "Viewport.h"

namespace ui {
constexpr uint32_t Background = 0x080F1B;
constexpr uint32_t Surface = 0x131F30;
constexpr uint32_t Accent = 0x53DDDF;
constexpr uint32_t Muted = 0x8395AB;
constexpr uint32_t White = 0xEBF3FA;
lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color = Surface);
lv_obj_t* label(lv_obj_t* parent, const char* text, int x, int y,
                uint32_t color = White, const lv_font_t* font = nullptr);
lv_obj_t* pageRoot();
void header(lv_obj_t* root, const char* title, const char* status);
void footer(lv_obj_t* root, const char* text);
void focus(lv_obj_t* object, bool selected);
void imageFit(lv_obj_t* image, const lv_img_dsc_t* source, int w, int h);
}
