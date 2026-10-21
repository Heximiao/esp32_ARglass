#pragma once
#include "CastTypes.h"
#include "../spi/St7789.h"
#include <Arduino.h>
namespace casting {
void begin(St7789& screen);
void tick();
bool start(NetworkMode mode);
void setAccepting(bool enabled);
bool accepting();
bool streaming();
bool running();
Options options();
void setOptions(const Options& value);
String stateJson(const char* type = "state", int ack = -1);
const char* status();
String address();
String hotspotName();
String hotspotPassword();
// Main-loop UI helpers; credentials are committed only after successful DHCP.
bool wifiConnected();
bool wifiConnecting();
String wifiName();
// Empty until the station has a usable DHCP address; not the hotspot address.
String localIp();
String savedWifiName();
void cancelWifi();
bool reconnectWifi();
// Pause boot/retry attempts while the user scans and edits credentials.
void setWifiSetupActive(bool active);
bool queueWifi(const char* ssid, const char* password);
// HTTP task exclusively owns a claimed input slot until submit/cancel.
uint8_t* claimFrame(int& slot);
uint8_t* frameBuffer(int slot);
void submitFrame(int slot, size_t bytes);
void cancelFrame(int slot);
void requestStop();
}
