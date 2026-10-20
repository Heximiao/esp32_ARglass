#include "UiApp.h"
#include "PhotoStore.h"
#include "pages/HomePage.h"
#include "pages/AlbumPage.h"
#include "pages/AnimationPage.h"
#include "pages/CastPage.h"
#include "pages/SettingsPage.h"
#include "pages/KeyDebugPage.h"
#include "pages/CastSettingsPage.h"
#include "pages/WifiPage.h"
#include "../cast/CastService.h"

namespace ui {
namespace {
Page current = Page::Home;
lv_obj_t* root = nullptr;
Preferences options;
}
Preferences& preferences() { return options; }
void navigate(Page page) {
    if(current==Page::Wifi) wifi::leave();
    if(page!=Page::Cast) casting::setAccepting(false);
    lv_obj_t* next = nullptr;
    switch (page) {
        case Page::Home: next = home::create(); break;
        case Page::Album: next = album::create(); break;
        case Page::Animation: next = animation::create(); break;
        case Page::Cast: next = cast::create(); break;
        case Page::Settings: next = settings::create(); break;
        case Page::KeyDebug: next = keydebug::create(); break;
        case Page::CastSettings: next = castsettings::create(current); break;
        case Page::Wifi: next = wifi::create(); break;
    }
    lv_obj_t* old = root ? root : lv_scr_act();
    lv_scr_load(next);
    if (old) lv_obj_del(old);
    root = next;
    current = page;
}
void begin() { photos().begin(); navigate(Page::Home); }
void tick() {
    if (current == Page::Animation) animation::tick();
    if (current == Page::KeyDebug) keydebug::tick();
    if (current == Page::Cast) cast::tick();
    if (current == Page::Wifi) wifi::tick();
}
void input(Input event, bool repeated) {
    // Animation directions toggle options; do not toggle repeatedly while held.
    if (repeated && current == Page::Animation) return;
    switch (current) {
        case Page::Home: home::input(event); break;
        case Page::Album: album::input(event); break;
        case Page::Animation: animation::input(event); break;
        case Page::Cast: cast::input(event); break;
        case Page::Settings: settings::input(event); break;
        case Page::KeyDebug: keydebug::input(event, repeated); break;
        case Page::CastSettings: castsettings::input(event); break;
        case Page::Wifi: wifi::input(event); break;
    }
}
}
