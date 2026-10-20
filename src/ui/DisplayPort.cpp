#include "DisplayPort.h"
#include "Viewport.h"
#include <lvgl.h>
#include <esp_heap_caps.h>

namespace {
St7789* display = nullptr;
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t driver;
uint32_t lastTick = 0;
bool videoMode = false;
void flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* colors) {
    if (videoMode) { lv_disp_flush_ready(drv); return; }
    // Translate the narrower logical display without resampling any pixels.
    const esp_err_t err = display->drawRect(ui::viewport::Left + area->x1,
        ui::viewport::Top + area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
        reinterpret_cast<const uint16_t*>(colors));
    if (err != ESP_OK) Serial.printf("Display flush failed: %s\n", esp_err_to_name(err));
    lv_disp_flush_ready(drv); // drawRect has already waited for DMA.
}
}
namespace ui {
void setVideoMode(bool enabled) {
    if (videoMode == enabled) return;
    videoMode = enabled;
    if (!enabled && lv_scr_act()) lv_obj_invalidate(lv_scr_act());
}
bool displayBegin(St7789& screen) {
    static_assert(sizeof(lv_color_t) == 2, "Driver requires native RGB565");
    lv_init();
    constexpr size_t pixels = viewport::Width * 32;
    auto* buffer = static_cast<lv_color_t*>(heap_caps_malloc(pixels * sizeof(lv_color_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer) return false;
    if (screen.fill(0x0000) != ESP_OK) {
        heap_caps_free(buffer);
        return false;
    }
    display = &screen;
    lv_disp_draw_buf_init(&drawBuffer, buffer, nullptr, pixels);
    lv_disp_drv_init(&driver);
    driver.hor_res = viewport::Width;
    driver.ver_res = viewport::Height;
    driver.draw_buf = &drawBuffer;
    driver.flush_cb = flush;
    lastTick = millis();
    if (!lv_disp_drv_register(&driver)) {
        heap_caps_free(buffer);
        return false;
    }
    Serial.printf("UI viewport: %ux%u at (%u,%u), native pixels / no scaling\n",
        viewport::Width, viewport::Height, viewport::Left, viewport::Top);
    return true;
}
void displayTick() {
    const uint32_t now = millis();
    lv_tick_inc(now - lastTick);
    lastTick = now;
    lv_timer_handler();
}
}
