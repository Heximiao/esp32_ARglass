#include "CastService.h"
#include "CastServer.h"
#include "WifiScan.h"
#include "StreamTiming.h"
#include "../ui/DisplayPort.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <JPEGDEC.h>
#include <atomic>
#include <new>
#include <cstring>
#include <algorithm>

namespace casting {
namespace {
enum class SlotState { Free, Writing, Ready, Reading };
struct Slot { uint8_t* data=nullptr; SlotState state=SlotState::Free; size_t size=0; uint32_t order=0; };
Slot slots[2];
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
Options config;
St7789* display=nullptr;
JPEGDEC* decoder=nullptr;
uint16_t* decoded=nullptr;
Preferences storage;
bool storageOpen=false, initialized=false, netStarted=false, apStarted=false, mdnsStarted=false;
bool wifiPending=false, configDirty=false;
NetworkMode preferredMode=NetworkMode::AccessPoint;
char pendingSsid[33]={}, pendingPassword[65]={};
String apName, apPassword;
String joiningSsid, joiningPassword;
bool joining=false;
bool autoWifiEnabled=true, wifiSetupActive=false;
uint32_t lastWifiAttempt=0;
constexpr uint32_t WifiRetryMs=30000;
const char* networkStatus="OFFLINE";
std::atomic<bool> enabled{false}, live{false}, stopRequested{false};
std::atomic<uint32_t> received{0}, dropped{0}, errors{0}, actualFps10{0}, lastReceive{0};
uint32_t arrivalOrder=0, connectAt=0, savedAt=0, frameAt=0, fpsAt=0, displayed=0;
bool fpsSampleReady=false;

bool allocate() {
    if (initialized) return true;
    for (auto& slot:slots) {
        if (!slot.data) slot.data=static_cast<uint8_t*>(heap_caps_malloc(HeaderBytes+MaxPayload,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if (!slot.data) return false;
    }
    if (!decoded) decoded=static_cast<uint16_t*>(heap_caps_malloc(RawBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if (!decoder) {
        void* memory=heap_caps_malloc(sizeof(JPEGDEC),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if (memory) decoder=new(memory) JPEGDEC();
    }
    initialized=decoded && decoder;
    return initialized;
}
void startAp() {
    autoWifiEnabled=false;
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_AP_STA);
    if (!apStarted) apStarted=WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0)) &&
        WiFi.softAP(apName.c_str(),apPassword.c_str(),1,false,2);
    networkStatus=apStarted ? "HOTSPOT READY" : "HOTSPOT FAILED";
}
void clearReady() {
    portENTER_CRITICAL(&mux);
    for (auto& slot:slots) if (slot.state==SlotState::Ready) slot.state=SlotState::Free;
    portEXIT_CRITICAL(&mux);
}
int drawJpeg(JPEGDRAW* block) {
    const int width=ui::viewport::Width, height=ui::viewport::Height;
    if (block->x<0 || block->y<0 || block->x>=width || block->y>=height || block->iWidth<=0) return 0;
    const int w=std::min(block->iWidthUsed,width-block->x);
    const int h=std::min(block->iHeight,height-block->y);
    if (w<=0 || h<=0 || w>block->iWidth) return 0;
    for (int y=0;y<h;++y) memcpy(decoded+(block->y+y)*width+block->x,
        block->pPixels+y*block->iWidth,w*2);
    return 1;
}
void fpsOverlay(uint16_t* pixels) {
    // Tiny native-pixel digits and FPS, avoiding a second LVGL full-frame flush.
    const uint16_t glyphs[]={0x7B6F,0x2492,0x73E7,0x73CF,0x5BC9,0x79CF,0x79EF,0x7249,0x7BEF,0x7BCF,
                             0x79E4,0x7BE4,0x79CF,0x01C0}; // digits / F / P / S / dash
    char text[16];
    if(fpsSampleReady)snprintf(text,sizeof(text),"%uFPS",unsigned((actualFps10.load()+5)/10));
    else strcpy(text,"--FPS"); // No completed measurement window yet; not a real 0 FPS.
    const int x0=ui::viewport::Width-int(strlen(text))*8-4;
    for (int y=0;y<14;++y) for(int x=x0-2;x<int(ui::viewport::Width);++x) pixels[y*ui::viewport::Width+x]=0;
    for (unsigned n=0;text[n];++n) {
        const char c=text[n]; const unsigned index=c>='0'&&c<='9'?c-'0':c=='F'?10:c=='P'?11:c=='S'?12:13;
        for(int y=0;y<5;++y) for(int x=0;x<3;++x) if(glyphs[index]&(1U<<(14-y*3-x)))
            for(int yy=0;yy<2;++yy) for(int xx=0;xx<2;++xx)
                pixels[(2+y*2+yy)*ui::viewport::Width+x0+n*8+x*2+xx]=0xFFFF;
    }
}
}
void begin(St7789& screen) {
    display=&screen;
    storageOpen=storage.begin("casting",false);
    if (storageOpen) {
        config.format=storage.getUChar("format",1)==2?Format::Rgb565:Format::Jpeg;
        config.fps=storage.getUChar("fps",30);
        config.quality=storage.getUChar("quality",70);
        config.fill=storage.getBool("fill",false);
        config.showFps=storage.getBool("info",true);
    }
    setOptions(config);
    const uint32_t identity=uint32_t(ESP.getEfuseMac());
    char name[28]; snprintf(name,sizeof(name),"AR-Glass-%04X",unsigned(identity&0xffff)); apName=name;
    char password[20]; snprintf(password,sizeof(password),"glass-%08x",unsigned(esp_random())); apPassword=password;
    // Queue only: connecting stays in tick(), so boot/UI are never blocked.
    lastWifiAttempt=millis();
    reconnectWifi();
}
Options options() { portENTER_CRITICAL(&mux); Options copy=config; portEXIT_CRITICAL(&mux); return copy; }
void setOptions(const Options& value) {
    Options next=value;
    if(next.format!=Format::Jpeg && next.format!=Format::Rgb565) next.format=Format::Jpeg;
    if(next.fps>50) next.fps=50;
    next.quality=std::max(uint8_t(40),std::min(next.quality,uint8_t(95)));
    portENTER_CRITICAL(&mux); config=next; configDirty=true; portEXIT_CRITICAL(&mux);
}
bool start(NetworkMode mode) {
    preferredMode=mode;
    if(mode==NetworkMode::Station){autoWifiEnabled=true;WiFi.setAutoReconnect(true);}
    if(!allocate()) { networkStatus="PSRAM ALLOCATION FAILED"; return false; }
    if(!netStarted) {
        WiFi.persistent(false); WiFi.setHostname("ar-glass"); WiFi.setAutoReconnect(true);
        if(mode==NetworkMode::AccessPoint) startAp();
        else {
            const String ssid=storageOpen?storage.getString("ssid",""):String();
            if(ssid.isEmpty()) { networkStatus="SELECT WIFI FIRST"; return false; }
            else {
                WiFi.enableSTA(true);
                if(!wifiConnected() && !wifiConnecting())reconnectWifi();
                networkStatus=wifiConnected()?"WIFI CONNECTED":"CONNECTING WIFI";
            }
        }
        WiFi.setSleep(false); esp_wifi_set_ps(WIFI_PS_NONE);
        if(!serverBegin()) { networkStatus="WEB SERVER FAILED"; return false; }
        netStarted=true;
    } else if(mode==NetworkMode::AccessPoint) startAp();
    else if(WiFi.status()!=WL_CONNECTED) {
        const String ssid=storageOpen?storage.getString("ssid",""):String();
        if(!ssid.isEmpty()) { if(!wifiConnecting())reconnectWifi(); networkStatus="CONNECTING WIFI"; }
        else { networkStatus="SELECT WIFI FIRST"; return false; }
    }
    clearReady(); stopRequested.store(false);
    received.store(0);dropped.store(0);errors.store(0);actualFps10.store(0);
    fpsSampleReady=false;
    setAccepting(true); return true;
}
void setAccepting(bool value) {
    enabled.store(value);
    if(!value) { stopRequested.store(true); live.store(false); ui::setVideoMode(false); }
}
bool accepting(){return enabled.load();}
bool streaming(){return live.load();}
bool running(){return netStarted;}
const char* status(){return networkStatus;}
String address(){return preferredMode==NetworkMode::Station && WiFi.status()==WL_CONNECTED?
    String("https://ar-glass.local"):apStarted?String("https://192.168.4.1"):String("https://ar-glass.local");}
String hotspotName(){return apStarted && preferredMode==NetworkMode::AccessPoint?apName:String();}
String hotspotPassword(){return apStarted?apPassword:String();}
bool wifiConnected(){return WiFi.status()==WL_CONNECTED && !joining && uint32_t(WiFi.localIP())!=0;}
bool wifiConnecting(){portENTER_CRITICAL(&mux);bool pending=wifiPending;portEXIT_CRITICAL(&mux);return joining || pending;}
String wifiName(){return WiFi.status()==WL_CONNECTED?WiFi.SSID():joiningSsid;}
String localIp(){
    if(!wifiConnected())return String();
    const IPAddress ip=WiFi.localIP();
    return uint32_t(ip)?ip.toString():String();
}
String savedWifiName(){return storageOpen?storage.getString("ssid",""):String();}
bool reconnectWifi(){
    const String name=savedWifiName();
    return !name.isEmpty() && queueWifi(name.c_str(),storage.getString("password","").c_str());
}
void cancelWifi(){
    lastWifiAttempt=millis();
    portENTER_CRITICAL(&mux); wifiPending=false; memset(pendingPassword,0,sizeof(pendingPassword)); portEXIT_CRITICAL(&mux);
    if(joining){WiFi.disconnect();joining=false;joiningPassword="";connectAt=0;networkStatus="WIFI CANCELLED";}
}
void setWifiSetupActive(bool active){
    wifiSetupActive=active;
    if(active)cancelWifi();
    else lastWifiAttempt=millis();
    WiFi.setAutoReconnect(!active && autoWifiEnabled);
}
bool queueWifi(const char* ssid,const char* password) {
    if(!ssid || !password || strlen(ssid)<1 || strlen(ssid)>32 || strlen(password)>64) return false;
    portENTER_CRITICAL(&mux);
    memcpy(pendingSsid,ssid,strlen(ssid)+1); memcpy(pendingPassword,password,strlen(password)+1); wifiPending=true;
    portEXIT_CRITICAL(&mux); return true;
}
uint8_t* claimFrame(int& index) {
    index=-1; portENTER_CRITICAL(&mux);
    for(int i=0;i<2;++i) if(slots[i].state==SlotState::Free) {index=i;break;}
    if(index<0) for(int i=0;i<2;++i) if(slots[i].state==SlotState::Ready) {index=i; dropped.fetch_add(1);break;}
    if(index>=0) slots[index].state=SlotState::Writing;
    portEXIT_CRITICAL(&mux);
    return index<0?nullptr:slots[index].data;
}
uint8_t* frameBuffer(int index) { return index>=0 && index<2?slots[index].data:nullptr; }
void cancelFrame(int index) {
    if(index<0 || index>1) return;
    portENTER_CRITICAL(&mux); slots[index].state=SlotState::Free; portEXIT_CRITICAL(&mux);
}
void submitFrame(int index,size_t bytes) {
    FrameInfo info;
    if(!accepting() || !parseFrame(slots[index].data,bytes,info)) { errors.fetch_add(1); cancelFrame(index); return; }
    portENTER_CRITICAL(&mux);
    slots[index].size=bytes; slots[index].order=++arrivalOrder; slots[index].state=SlotState::Ready;
    portEXIT_CRITICAL(&mux);
    received.fetch_add(1); lastReceive.store(millis());
}
void requestStop(){stopRequested.store(true);}
String stateJson(const char* type,int ack) {
    const auto o=options();
    char json[430]; snprintf(json,sizeof(json),
        "{\"type\":\"%s\",\"ack\":%d,\"width\":%u,\"height\":%u,\"format\":%u,\"fps\":%u,\"quality\":%u,\"fill\":%s,\"showFps\":%s,\"actualFps\":%.1f,\"dropped\":%u,\"errors\":%u,\"accepting\":%s,\"streaming\":%s}",
        type,ack,ui::viewport::Width,ui::viewport::Height,unsigned(o.format),o.fps,o.quality,o.fill?"true":"false",
        o.showFps?"true":"false",actualFps10.load()/10.0,unsigned(dropped.load()),unsigned(errors.load()),
        accepting()?"true":"false",streaming()?"true":"false"); return String(json);
}
void tick() {
    networks::tick();
    const uint32_t now=millis();
    bool save=false, changeWifi=false; char ssid[33],password[65];
    portENTER_CRITICAL(&mux);
    if(configDirty && uint32_t(now-savedAt)>=500) {save=true;configDirty=false;}
    if(wifiPending) {memcpy(ssid,pendingSsid,sizeof(ssid));memcpy(password,pendingPassword,sizeof(password));wifiPending=false;memset(pendingPassword,0,sizeof(pendingPassword));changeWifi=true;}
    portEXIT_CRITICAL(&mux);
    if(save && storageOpen) {
        const auto o=options(); storage.putUChar("format",unsigned(o.format)); storage.putUChar("fps",o.fps);
        storage.putUChar("quality",o.quality); storage.putBool("fill",o.fill); storage.putBool("info",o.showFps); savedAt=now;
    }
    if(changeWifi) {
        autoWifiEnabled=true;lastWifiAttempt=now;
        joiningSsid=ssid; joiningPassword=password; joining=true; preferredMode=NetworkMode::Station;
        WiFi.persistent(false); WiFi.setHostname("ar-glass"); WiFi.setAutoReconnect(!wifiSetupActive);
        WiFi.enableSTA(true); WiFi.disconnect(); WiFi.begin(ssid,password);
        WiFi.setSleep(false); connectAt=now; networkStatus="CONNECTING WIFI";
        memset(password,0,sizeof(password));
    }
    if(joining) {
        if(WiFi.status()==WL_CONNECTED && WiFi.SSID()==joiningSsid && uint32_t(WiFi.localIP())!=0) {
            if(storageOpen){storage.putString("ssid",joiningSsid);storage.putString("password",joiningPassword);}
            joiningPassword="";joining=false;connectAt=0;networkStatus="WIFI CONNECTED";
        } else if(uint32_t(now-connectAt)>=20000) {
            WiFi.disconnect();joiningPassword="";joining=false;connectAt=0;networkStatus="WIFI FAILED";
        }
    }
    if(autoWifiEnabled && !wifiSetupActive && !wifiConnected() && !wifiConnecting() &&
       !networks::scanning() && uint32_t(now-lastWifiAttempt)>=WifiRetryMs){
        lastWifiAttempt=now;
        reconnectWifi();
    }
    if(netStarted) {
        if(!joining && WiFi.status()==WL_CONNECTED) {networkStatus="WIFI CONNECTED";connectAt=0;}
        else if(!joining && connectAt && uint32_t(now-connectAt)>20000) {
            connectAt=0;networkStatus="WIFI FAILED";
        }
        if(!mdnsStarted && (apStarted || WiFi.status()==WL_CONNECTED)) {
            mdnsStarted=MDNS.begin("ar-glass");
            if(mdnsStarted) MDNS.addService("https","tcp",443);
        }
    }
    // Read activity before taking a fresh clock sample. The top-of-loop `now`
    // may precede a frame received concurrently while Wi-Fi/NVS work was running.
    const uint32_t receivedAt=lastReceive.load();
    if(stopRequested.exchange(false) || (live.load() && timing::idle(millis(),receivedAt))) {
        live.store(false); actualFps10.store(0); fpsSampleReady=false;
        clearReady(); ui::setVideoMode(false);
    }
    if(!accepting()) return;
    const auto o=options();
    if(o.fps && uint32_t(now-frameAt)<1000/o.fps) return;
    int selected=-1;
    portENTER_CRITICAL(&mux);
    for(int i=0;i<2;++i) if(slots[i].state==SlotState::Ready &&
        (selected<0 || int32_t(slots[i].order-slots[selected].order)>0)) selected=i;
    if(selected>=0) {
        slots[selected].state=SlotState::Reading;
        for(int i=0;i<2;++i) if(i!=selected && slots[i].state==SlotState::Ready) {slots[i].state=SlotState::Free;dropped.fetch_add(1);}
    }
    portEXIT_CRITICAL(&mux);
    if(selected<0) return;
    auto& slot=slots[selected]; FrameInfo info; bool okay=parseFrame(slot.data,slot.size,info);
    uint16_t* pixels=decoded;
    if(okay && info.format==Format::Jpeg) {
        okay=decoder->openRAM(slot.data+HeaderBytes,info.bytes,drawJpeg);
        if(okay) {
            okay=decoder->getWidth()==int(ui::viewport::Width) && decoder->getHeight()==int(ui::viewport::Height) &&
                 decoder->getJPEGType()==JPEG_MODE_BASELINE;
            decoder->setPixelType(RGB565_LITTLE_ENDIAN);
            if(okay) okay=decoder->decode(0,0,0);
            decoder->close();
        }
    } else if(okay) pixels=reinterpret_cast<uint16_t*>(slot.data+HeaderBytes);
    if(okay) {
        ui::setVideoMode(true);
        if(o.showFps) fpsOverlay(pixels);
        okay=display->drawRect(ui::viewport::Left,ui::viewport::Top,ui::viewport::Width,ui::viewport::Height,pixels)==ESP_OK;
        const uint32_t presentedAt=millis();
        frameAt=presentedAt;
        if(okay){
            if(!live.exchange(true)) { fpsAt=presentedAt; displayed=0; }
            ++displayed;
            if(uint32_t(presentedAt-fpsAt)>=1000) {
                actualFps10.store(displayed*10000/(presentedAt-fpsAt));
                displayed=0;fpsAt=presentedAt;fpsSampleReady=true;
            }
        }else if(!live.load())ui::setVideoMode(false);
    }
    if(!okay) errors.fetch_add(1);
    cancelFrame(selected);
}
}
