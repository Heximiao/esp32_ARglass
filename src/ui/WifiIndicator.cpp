#include "WifiIndicator.h"
#include "Widgets.h"
#include "../cast/CastService.h"
#include <Arduino.h>

namespace ui { namespace wifiindicator {
namespace {
lv_obj_t* icon=nullptr;
int previous=-1;
}
void create(lv_obj_t* parent){
    icon=label(parent,LV_SYMBOL_WIFI,0,0,Muted,&lv_font_montserrat_14);
    // Between the title and existing card counter, inside the AR safe viewport.
    lv_obj_align(icon,LV_ALIGN_TOP_RIGHT,-76,11);
    previous=-1;tick();
}
void tick(){
    const int state=casting::wifiConnected()?2:
        casting::wifiConnecting()?(millis()/500%2?1:0):0;
    if(state==previous)return;
    previous=state;
    lv_obj_set_style_text_color(icon,lv_color_hex(state?Accent:Muted),0);
    lv_obj_set_style_text_opa(icon,state?LV_OPA_COVER:LV_OPA_30,0);
}
} }
