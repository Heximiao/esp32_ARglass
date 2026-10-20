#include "Joystick.h"
#include "JoystickConfig.h"
#include "../ui/UiApp.h"
#include <Arduino.h>

namespace joystick {
namespace {
struct Key {
    uint8_t pin;
    bool raw=false, pressed=false, rose=false, fell=false;
    uint32_t changedAt=0, pressedAt=0;
    explicit Key(uint8_t gpio) : pin(gpio) {}
    void sample(uint32_t now) {
        rose = fell = false;
        const bool value = digitalRead(pin) == LOW;
        if (value != raw) { raw = value; changedAt = now; }
        // Filter BOTH press and release: every edge must remain stable for 25 ms.
        if (raw != pressed && uint32_t(now - changedAt) >= config::DebounceMs) {
            pressed = raw;
            if (pressed) { rose = true; pressedAt = now; }
            else fell = true;
        }
    }
};
Key keys[] = {Key(config::UpPin), Key(config::DownPin), Key(config::LeftPin),
              Key(config::RightPin), Key(config::MidPin)};
constexpr ui::Input Directions[] = {ui::Input::Up, ui::Input::Down, ui::Input::Left, ui::Input::Right};
bool armed=false, midCycle=false, backSent=false, repeating=false;
uint32_t quietSince=0, directionSince=0, lastRepeat=0;
int activeDirection=-1;
}
void begin() {
    const uint32_t now = millis();
    for (auto& key : keys) {
        pinMode(key.pin, INPUT_PULLUP);
        key.raw = digitalRead(key.pin) == LOW;
        key.pressed = key.rose = key.fell = false;
        key.changedAt = key.pressedAt = now;
    }
    // Do not act on keys held during boot; wait until all switches are released.
    armed = midCycle = backSent = repeating = false;
    activeDirection = -1;
    quietSince = now;
    Serial.println("Joystick: COM=GND UP=12 DOWN=13 LEFT=14 RIGHT=15 MID=16");
}
void tick() {
    const uint32_t now = millis();
    bool anyHeld = false;
    for (auto& key : keys) {
        key.sample(now);
        anyHeld = anyHeld || key.raw || key.pressed;
    }
    if (!armed) {
        if (anyHeld) quietSince = now;
        else if (uint32_t(now - quietSince) >= config::DebounceMs) armed = true;
        return;
    }
    auto& mid = keys[4];
    if (mid.rose) { midCycle = true; backSent = false; }
    if (mid.fell) {
        const bool confirm = midCycle && !backSent;
        midCycle = false;
        activeDirection = -1;
        if (confirm) ui::input(ui::Input::Confirm);
        return;
    }
    if (mid.pressed || midCycle) {
        activeDirection = -1;
        if (midCycle && mid.raw && !backSent &&
                uint32_t(now - mid.pressedAt) >= config::LongPressMs) {
            backSent = true;
            ui::input(ui::Input::Back);
        }
        return;
    }
    int direction = -1;
    unsigned count = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (keys[i].pressed) { direction = i; ++count; }
    }
    // Ignore diagonal/opposing combinations rather than sending two events.
    if (count != 1) { activeDirection = -1; return; }
    // Release bounce must not restart the held key or generate a new press.
    if (!keys[direction].raw) return;
    if (direction != activeDirection) {
        activeDirection = direction;
        directionSince = lastRepeat = now;
        repeating = false;
        ui::input(Directions[direction]);
    } else if ((!repeating && uint32_t(now - directionSince) >= config::RepeatDelayMs) ||
               (repeating && uint32_t(now - lastRepeat) >= config::RepeatIntervalMs)) {
        repeating = true;
        lastRepeat = now; // Never send a catch-up burst after a slow display update.
        ui::input(Directions[direction], true);
    }
}
}
