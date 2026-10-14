#pragma once
#include <Arduino.h>
#include <driver/spi_master.h>

class St7789 {
public:
    static constexpr uint16_t Width = 320, Height = 172;
    struct Config {
        int scl=1, sda=2, res=3, dc=4, cs=5, bl=6;
        int clockHz=40000000;
        int backlightPwmChannel=0;
        // Landscape: the panel's 34-pixel inset is on the row axis.
        uint16_t xOffset=0, yOffset=34;
        bool bgr=false, inverted=true, backlightActiveHigh=true;
    };
    St7789() = default;
    St7789(const St7789&) = delete;
    St7789& operator=(const St7789&) = delete;
    esp_err_t begin(const Config& config);
    void end();
    void setBacklight(bool on);
    void setBrightness(uint8_t percent);
    // Packed native RGB565 (red=0xF800), PSRAM allowed. Blocking until DMA done.
    // Single-task API; source must not change during this call.
    esp_err_t drawRect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t* pixels);
    esp_err_t fill(uint16_t color);
private:
    static constexpr size_t ChunkPixels=Width*16;
    Config cfg_;
    spi_device_handle_t dev_=nullptr;
    bool owned_=false, ready_=false;
    bool backlightPwmReady_=false;
    uint8_t brightness_=75;
    uint8_t* buf_[2]={nullptr,nullptr};
    esp_err_t command(uint8_t cmd,const uint8_t* data=nullptr,size_t len=0);
    esp_err_t window(uint16_t x,uint16_t y,uint16_t w,uint16_t h);
    esp_err_t transfer(const uint16_t* pixels,size_t count,uint16_t color);
};
