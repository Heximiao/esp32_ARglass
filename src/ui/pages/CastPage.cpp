#include "CastPage.h"
#include "../Widgets.h"
#include "../../cast/CastService.h"
#include <Arduino.h>

namespace ui { namespace cast {
namespace {
lv_obj_t* root;lv_obj_t* list;lv_obj_t* rows[5];lv_obj_t* values[5];lv_obj_t* info[6];
unsigned selected=0;bool waiting=false,startFailed=false;uint32_t updatedAt=0;
casting::NetworkMode mode=casting::NetworkMode::Station;
void refresh(lv_anim_enable_t animate=LV_ANIM_ON) {
    lv_label_set_text(values[0],mode==casting::NetworkMode::AccessPoint?"HOTSPOT":"Wi-Fi");
    const auto o=casting::options();
    if(o.fps)lv_label_set_text_fmt(values[1],"%u FPS",o.fps);else lv_label_set_text(values[1],"AUTO");
    for(unsigned i=0;i<5;++i){focus(rows[i],selected==i);lv_obj_align(values[i],LV_ALIGN_RIGHT_MID,-12,0);}
    lv_obj_update_layout(list);lv_obj_scroll_to_view(rows[selected],animate);
}
void render() {
    lv_obj_clean(root);
    header(root,"投屏",waiting?"BROWSER CAST":startFailed?casting::status():"VIDEO ONLY");
    if(waiting){
        for(unsigned i=0;i<6;++i)info[i]=label(root,"",12,36+i*17,White,&lv_font_montserrat_12);
        footer(root,"MID stop  HOLD menu");updatedAt=0;
    }else{
        list=scrollArea(root,12,35,viewport::ContentWidth,110,LV_DIR_VER);
        const char* names[]={"连接方式","目标帧率","投屏设置","连接 Wi-Fi","开始投屏"};
        for(unsigned i=0;i<5;++i){rows[i]=panel(list,0,i*36,viewport::ContentWidth,32);
            label(rows[i],names[i],12,4);values[i]=label(rows[i],i>=2?"OPEN >":"",0,0,Accent,&lv_font_montserrat_12);}
        refresh(LV_ANIM_OFF);footer(root,"U/D select  MID open  HOLD back");
    }
}
}
lv_obj_t* create(){root=pageRoot();waiting=false;startFailed=false;render();return root;}
void input(Input event) {
    if(waiting){
        if(event==Input::Back||event==Input::Confirm){casting::setAccepting(false);waiting=false;render();}
        return;
    }
    if(event==Input::Back){navigate(Page::Home);return;}
    if(event==Input::Up)selected=(selected+4)%5;
    if(event==Input::Down)selected=(selected+1)%5;
    if(event==Input::Left||event==Input::Right||event==Input::Confirm){
        if(selected==0)mode=mode==casting::NetworkMode::AccessPoint?casting::NetworkMode::Station:casting::NetworkMode::AccessPoint;
        if(selected==1){const uint8_t rates[]={5,10,15,20,25,30,40,50,0};auto o=casting::options();unsigned index=0;
            for(unsigned i=0;i<9;++i)if(rates[i]==o.fps)index=i;
            o.fps=rates[(index+(event==Input::Left?8:1))%9];casting::setOptions(o);}
        if(selected==2 && event!=Input::Left){navigate(Page::CastSettings);return;}
        if(selected==3 && event==Input::Confirm){mode=casting::NetworkMode::Station;navigate(Page::Wifi);return;}
        if(selected==4 && event==Input::Confirm){
            if(mode==casting::NetworkMode::Station && casting::savedWifiName().isEmpty() && !casting::wifiConnected()){navigate(Page::Wifi);return;}
            waiting=casting::start(mode);startFailed=!waiting;render();
            return;
        }
    }
    refresh();
}
void tick(){
    if(!waiting||casting::streaming()||uint32_t(millis()-updatedAt)<300)return;
    updatedAt=millis();
    lv_label_set_text(info[0],casting::address().c_str());
    const String ssid=casting::hotspotName();
    lv_label_set_text_fmt(info[1],"SSID: %s",ssid.isEmpty()?"same 2.4 GHz WiFi":ssid.c_str());
    lv_label_set_text_fmt(info[2],"PASS: %s",ssid.isEmpty()?"saved network":casting::hotspotPassword().c_str());
    lv_label_set_text_fmt(info[3],"PAIR CODE: %s",casting::pin());
    lv_label_set_text(info[4],casting::status());
    lv_label_set_text(info[5],ssid.isEmpty()?"PC: same Wi-Fi / trust HTTPS":"First: http://192.168.4.1");
}
} }
