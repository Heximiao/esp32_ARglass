#include "SerialPreview.h"
#include "UiApp.h"
#include <Arduino.h>

namespace ui {
void serialPreviewTick() {
    // No GPIOs or joystick assumptions. All UI calls stay on the loop task.
    // Ignore line endings so sending "1" + Enter only selects the album.
    for (unsigned n = 0; n < 16 && Serial.available(); ++n) {
        const char c = Serial.read();
        switch (c) {
            case '0': navigate(Page::Home); break;
            case '1': navigate(Page::Album); break;
            case '2': navigate(Page::Animation); break;
            case '3': navigate(Page::Cast); break;
            case '4': navigate(Page::Settings); break;
            case 'w': case 'W': input(Input::Up); break;
            case 's': case 'S': input(Input::Down); break;
            case 'a': case 'A': input(Input::Left); break;
            case 'd': case 'D': input(Input::Right); break;
            case 'e': case 'E': input(Input::Confirm); break;
            case 'b': case 'B': case 27: input(Input::Back); break;
            case '?': Serial.println("UI: 0 home, 1 album, 2 animation, 3 cast, 4 settings; WASD, E confirm, B back"); break;
        }
    }
}
}
