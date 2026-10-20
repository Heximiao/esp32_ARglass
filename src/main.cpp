#include <Arduino.h>
#include <esp32-hal-rgb-led.h>
#include "spi/St7789.h"
#include "ui/DisplayPort.h"
#include "ui/UiApp.h"
#include "ui/SerialPreview.h"
#include "input/Joystick.h"

namespace {
St7789 screen;
bool ready = false;
constexpr uint8_t RgbLedPin = 38;
void updateRgbLed() {
    static uint32_t changedAt = 0;
    static bool on = false;
    const uint32_t now = millis();
    if (now - changedAt >= 1000) {
        changedAt = now; on = !on;
        // Preserve the board's RGB/GRB compensation from the original demo.
        neopixelWrite(RgbLedPin, on ? 16 : 0, 0, 0);
    }
}
}
void setup() {
    Serial.begin(115200);
    neopixelWrite(RgbLedPin, 0, 0, 0);
    St7789::Config config;
    ESP_ERROR_CHECK(screen.begin(config));
    if (!ui::displayBegin(screen)) {
        Serial.println("UI display initialization failed: check PSRAM.");
        screen.fill(0xF800);
        return;
    }
    ui::begin();
    joystick::begin();
    ready = true;
    Serial.println("AR Glass UI ready: 0 home / 1 album / 2 animation / 3 cast / 4 settings / 5 key test");
    Serial.println("Joystick: directions move, MID click confirms, hold MID 650ms to go back.");
    Serial.println("Serial preview: WASD move, E confirm, B back.");
}
void loop() {
    updateRgbLed();
    if (ready) {
        joystick::tick();
        ui::serialPreviewTick();
        static unsigned appliedBrightness = 75;
        const unsigned brightness = ui::preferences().brightness;
        if (brightness != appliedBrightness) {
            screen.setBrightness(brightness);
            appliedBrightness = brightness;
        }
        ui::tick();
        ui::displayTick();
    }
    delay(3);
}
