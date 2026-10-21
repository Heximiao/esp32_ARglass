#include "WifiPage.h"
#include "../Widgets.h"
#include "../PasswordKeyboard.h"
#include "../../cast/CastService.h"
#include "../../cast/WifiScan.h"
#include <Arduino.h>
#include <cstring>
namespace ui { namespace wifi {
namespace {
enum class State { List, Password, Connecting, Result };
State state=State::List;
lv_obj_t* root;lv_obj_t* list;lv_obj_t* rows[26];lv_obj_t* feedback=nullptr;
lv_obj_t* ipLabel=nullptr;
unsigned selected=0,total=0;bool wasScanning=false,success=false,openNetwork=false,unsupported=false;
String ssid;uint32_t updatedAt=0;
void refresh(){
    for(unsigned i=0;i<total;++i)focus(rows[i],i==selected);
    lv_obj_update_layout(list);lv_obj_scroll_to_view(rows[selected],LV_ANIM_ON);
}
void renderList(){
    state=State::List;lv_obj_clean(root);total=0;
    header(root,"连接 Wi-Fi",casting::networks::scanning()?"SCANNING":"2.4 GHz");
    list=scrollArea(root,12,35,viewport::ContentWidth,110,LV_DIR_VER);
    const auto add=[](const char* title,const char* detail){
        auto* r=rows[total]=panel(list,0,total*36,viewport::ContentWidth,32);
        auto* n=label(r,title,8,5);lv_obj_set_width(n,viewport::ContentWidth-88);lv_label_set_long_mode(n,LV_LABEL_LONG_DOT);
        auto* v=label(r,detail,0,0,Accent,&lv_font_montserrat_12);lv_obj_align(v,LV_ALIGN_RIGHT_MID,-6,0);++total;
    };
    add("刷新列表",casting::networks::scanning()?"WAIT":"SCAN");
    const String saved=casting::savedWifiName();
    add(saved.isEmpty()?"暂无保存网络":saved.c_str(),casting::wifiConnected()?"ONLINE":"SAVED");
    if(!casting::networks::scanning())for(unsigned i=0;i<casting::networks::count();++i){
        const auto& n=casting::networks::at(i);char detail[24];
        snprintf(detail,sizeof(detail),"%d %s",n.rssi,!n.supported?"N/A":n.open?"OPEN":"LOCK");
        add(n.ssid.c_str(),detail);
    }
    if(!casting::networks::scanning() && !casting::networks::count()){
        auto* hint=label(list,casting::networks::failed()?"扫描失败，请刷新":"未找到网络，请刷新",8,76,Muted);(void)hint;
    }
    if(selected>=total)selected=0;refresh();footer(root,"U/D select  MID join  HOLD back");
}
void message(const char* title,const char* body,const char* hint){
    lv_obj_clean(root);header(root,title,success?"ONLINE":"Wi-Fi");
    auto* name=label(root,ssid.c_str(),12,38,Accent);lv_obj_set_width(name,viewport::ContentWidth);lv_label_set_long_mode(name,LV_LABEL_LONG_DOT);
    feedback=label(root,body,12,65);lv_obj_set_width(feedback,viewport::ContentWidth);
    footer(root,hint);
}
void updateIp(){
    const String ip=casting::localIp();
    lv_label_set_text_fmt(ipLabel,"LAN IP: %s",ip.isEmpty()?"not connected":ip.c_str());
}
void result(bool ok,const char* text){
    state=State::Result;success=ok;
    message(ok?"连接成功":"连接失败",text,ok?"MID cast menu  HOLD networks":"MID retry  HOLD networks");
    if(ok){
        ipLabel=label(root,"",12,109,Accent,&lv_font_montserrat_14);
        lv_obj_set_width(ipLabel,viewport::ContentWidth);updateIp();
    }
}
void connect(const String& password){
    if(!casting::queueWifi(ssid.c_str(),password.c_str())){result(false,"网络名称或密码无效");return;}
    state=State::Connecting;success=false;updatedAt=millis();message("正在连接","正在获取网络地址…","HOLD cancel");
}
void passwordPage(){
    state=State::Password;lv_obj_clean(root);
    keyboard::create(root,ssid.c_str());footer(root,"MID type  JOIN connect  HOLD back");
}
}
lv_obj_t* create(){
    casting::setWifiSetupActive(true);
    root=pageRoot();selected=0;success=false;
    casting::networks::scan();wasScanning=casting::networks::scanning();renderList();return root;
}
void leave(){
    keyboard::clear();if(state==State::Connecting)casting::cancelWifi();
    casting::setWifiSetupActive(false);
}
void tick(){
    if(state==State::Result && success && uint32_t(millis()-updatedAt)>=500){
        updatedAt=millis();updateIp();
    }
    if(state==State::List && wasScanning!=casting::networks::scanning()){
        wasScanning=casting::networks::scanning();renderList();
    }
    if(state==State::Connecting && uint32_t(millis()-updatedAt)>100){
        if(casting::wifiConnected() && casting::wifiName()==ssid){result(true,"已保存，下次可直接连接");return;}
        if(!casting::wifiConnecting())result(false,"密码错误或信号较弱\n请检查后重试");
    }
}
void input(Input event){
    if(event==Input::Back){
        if(state==State::List){navigate(Page::Cast);return;}
        if(state==State::Connecting)casting::cancelWifi();
        keyboard::clear();renderList();return;
    }
    if(state==State::Password){
        if(keyboard::input(event)){
            const String password=keyboard::value();
            bool hex=password.length()==64;
            for(unsigned i=0;hex && i<password.length();++i)hex=strchr("0123456789abcdefABCDEF",password[i])!=nullptr;
            if(password.length()<8 || (password.length()==64 && !hex)){
                // Preserve the draft and cursor while explaining the invalid length.
                keyboard::error(password.length()<8?"至少输入 8 位":"64 位须为十六进制");return;
            }
            connect(password);keyboard::clear();
        }
        return;
    }
    if(state==State::Connecting)return;
    if(state==State::Result){
        if(event==Input::Confirm){if(success)navigate(Page::Cast);else if(unsupported)renderList();else if(openNetwork)connect("");else passwordPage();}return;
    }
    if(event==Input::Up)selected=(selected+total-1)%total;
    if(event==Input::Down)selected=(selected+1)%total;
    if(event==Input::Confirm){
        if(casting::networks::scanning() || casting::wifiConnecting())return;
        if(selected==0){casting::networks::scan();wasScanning=casting::networks::scanning();renderList();return;}
        if(selected==1){
            ssid=casting::savedWifiName();openNetwork=false;unsupported=false;if(ssid.isEmpty())return;
            // Re-use saved credentials inside the service; never display them.
            if(casting::reconnectWifi()){
                state=State::Connecting;success=false;updatedAt=millis();message("正在连接","正在连接保存的网络…","HOLD cancel");
            }return;
        }
        const auto& n=casting::networks::at(selected-2);ssid=n.ssid;openNetwork=n.open;unsupported=!n.supported;
        if(!n.supported){result(false,"此网络需特殊认证\n请选择个人 Wi-Fi 网络");return;}
        if(n.open)connect("");else passwordPage();return;
    }
    refresh();
}
} }
