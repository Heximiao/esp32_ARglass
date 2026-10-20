#include "WifiScan.h"
#include "CastService.h"
#include <WiFi.h>
#include <algorithm>
namespace casting { namespace networks {
namespace {
Network items[24];
unsigned size=0;
bool busy=false, error=false;
}
bool scan() {
    if(busy || wifiConnecting()) return false;
    size=0; error=false;
    WiFi.persistent(false);
    const int result=WiFi.scanNetworks(true,false,false,180);
    busy=result==WIFI_SCAN_RUNNING;
    error=result==WIFI_SCAN_FAILED;
    if(result>=0){busy=true;tick();}
    return !error;
}
void tick() {
    if(!busy) return;
    const int result=WiFi.scanComplete();
    if(result==WIFI_SCAN_RUNNING) return;
    busy=false;error=result<0;
    for(int i=0;i<result;++i) {
        const String ssid=WiFi.SSID(i);if(ssid.isEmpty())continue;
        const auto auth=WiFi.encryptionType(i);
        const bool supported=auth==WIFI_AUTH_OPEN || auth==WIFI_AUTH_WPA_PSK || auth==WIFI_AUTH_WPA2_PSK ||
            auth==WIFI_AUTH_WPA_WPA2_PSK || auth==WIFI_AUTH_WPA3_PSK || auth==WIFI_AUTH_WPA2_WPA3_PSK;
        Network candidate{ssid,WiFi.RSSI(i),auth==WIFI_AUTH_OPEN,supported};
        unsigned index=0;for(;index<size;++index)if(items[index].ssid==ssid)break;
        if(index<size){if(candidate.rssi>items[index].rssi)items[index]=candidate;}
        else if(size<24)items[size++]=candidate;
        else if(candidate.rssi>items[size-1].rssi)items[size-1]=candidate;
        std::sort(items,items+size,[](const Network& a,const Network& b){return a.rssi>b.rssi;});
    }
    WiFi.scanDelete();
}
bool scanning(){return busy;}
bool failed(){return error;}
unsigned count(){return size;}
const Network& at(unsigned index){return items[index];}
} }
