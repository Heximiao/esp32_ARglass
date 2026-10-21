#include "CastServer.h"
#include "CastService.h"
#include "private/TlsMaterial.h"
#include <CastWebPage.h>
#include <esp_https_server.h>
#include <esp_http_server.h>
#include <cJSON.h>
#include <lwip/sockets.h>
#include <freertos/semphr.h>
#include <cstring>

namespace casting {
namespace {
httpd_handle_t secureServer=nullptr, plainServer=nullptr;
SemaphoreHandle_t wsMutex=nullptr;
int ownerFd=-1;
httpd_handle_t ownerServer=nullptr;
int inboundSlot=-1;
size_t inboundBytes=0;
bool textInProgress=false;
size_t textBytes=0;
char textParts[1537]={};
void cancelInbound() {
    if(inboundSlot>=0)cancelFrame(inboundSlot);
    inboundSlot=-1;inboundBytes=0;
    textInProgress=false;textBytes=0;
}
const char SetupPage[] PROGMEM=R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>AR Glass · 首次连接</title><body style="max-width:680px;margin:60px auto;padding:24px;font:17px system-ui;background:#0b1423;color:#eaf4ff;line-height:1.8"><h1>AR Glass 投屏</h1><p>浏览器屏幕共享需要可信的 HTTPS。首次连接请下载本设备项目的根证书，对照项目 certs/fingerprint.txt 核验，并手动导入当前用户的“受信任的根证书颁发机构”。只对自己的设备进行此操作。网页不会自动安装证书。</p><p><a style="color:#60e2df" href="/root.crt" download="ar-glass-root.crt">下载根证书</a></p><p>然后在设备热点下打开 <a style="color:#60e2df" href="https://192.168.4.1/">https://192.168.4.1</a>；连接路由器时打开 <a style="color:#60e2df" href="https://ar-glass.local/">https://ar-glass.local</a>。</p><p>电脑 Chrome / Edge 支持选择屏幕、窗口或标签页。每次共享都需要点击按钮并授权；不采集音频。</p></body></html>)HTML";
esp_err_t closeResponse(httpd_req_t* req,const char* data,size_t size,const char* contentType) {
    httpd_resp_set_type(req,contentType);
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"Connection","close");
    httpd_resp_send(req,data,size);
    return ESP_FAIL; // Close idle page connections to leave internal RAM for WSS.
}
esp_err_t setupPage(httpd_req_t* req) {return closeResponse(req,SetupPage,strlen(SetupPage),"text/html; charset=utf-8");}
esp_err_t browserPage(httpd_req_t* req) {
    httpd_resp_set_hdr(req,"Content-Encoding","gzip");
    httpd_resp_set_hdr(req,"Permissions-Policy","microphone=(), camera=()");
    return closeResponse(req,reinterpret_cast<const char*>(CastPageGzip),sizeof(CastPageGzip),"text/html; charset=utf-8");
}
esp_err_t rootCertificate(httpd_req_t* req) {
    httpd_resp_set_hdr(req,"Content-Disposition","attachment; filename=ar-glass-root.crt");
    return closeResponse(req,CastRootCA,strlen(CastRootCA),"application/x-x509-ca-cert");
}
esp_err_t stateGet(httpd_req_t* req) {
    const String state=stateJson();
    return closeResponse(req,state.c_str(),state.length(),"application/json");
}
esp_err_t favicon(httpd_req_t* req) {httpd_resp_set_status(req,"204 No Content");return closeResponse(req,"",0,"image/x-icon");}
esp_err_t sendText(httpd_req_t* req,const String& text) {
    httpd_ws_frame_t packet={};packet.type=HTTPD_WS_TYPE_TEXT;
    packet.payload=reinterpret_cast<uint8_t*>(const_cast<char*>(text.c_str()));packet.len=text.length();
    return httpd_ws_send_frame(req,&packet);
}
esp_err_t wsLocked(httpd_req_t* req) {
    const int fd=httpd_req_to_sockfd(req);
    if(req->method==HTTP_GET) {
        if(!accepting()) {
            httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Start casting on the glasses first");return ESP_FAIL;
        }
        if(ownerServer && ownerFd>=0 && httpd_ws_get_fd_info(ownerServer,ownerFd)==HTTPD_WS_CLIENT_WEBSOCKET &&
           (ownerFd!=fd || ownerServer!=req->handle)) {
            httpd_resp_set_status(req,"409 Conflict");httpd_resp_sendstr(req,"One sender at a time");return ESP_FAIL;
        }
        const int noDelay=1;
        setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&noDelay,sizeof(noDelay));
        cancelInbound();ownerFd=fd;ownerServer=req->handle;
        return ESP_OK;
    }
    if(fd!=ownerFd || req->handle!=ownerServer) return ESP_FAIL;
    httpd_ws_frame_t packet={};
    if(httpd_ws_recv_frame(req,&packet,0)!=ESP_OK) {cancelInbound();ownerFd=-1;requestStop();return ESP_FAIL;}
    if(packet.type==HTTPD_WS_TYPE_CLOSE) {cancelInbound();ownerFd=-1;requestStop();return ESP_FAIL;}
    if(packet.type==HTTPD_WS_TYPE_BINARY || (packet.type==HTTPD_WS_TYPE_CONTINUE && !textInProgress)) {
        // Browsers may fragment large send(ArrayBuffer) messages, especially RGB565.
        if(packet.type==HTTPD_WS_TYPE_BINARY) {
            if(inboundSlot>=0 || textInProgress) {cancelInbound();return ESP_FAIL;}
            if(!claimFrame(inboundSlot))return ESP_FAIL;
            inboundBytes=0;
        } else if(inboundSlot<0) return ESP_FAIL;
        if(packet.len>HeaderBytes+MaxPayload-inboundBytes){cancelInbound();return ESP_FAIL;}
        // A claimed slot stays Writing across every continuation fragment.
        uint8_t* data=frameBuffer(inboundSlot);
        packet.payload=data+inboundBytes;
        if(packet.len && httpd_ws_recv_frame(req,&packet,HeaderBytes+MaxPayload-inboundBytes)!=ESP_OK){cancelInbound();return ESP_FAIL;}
        inboundBytes+=packet.len;
        if(!packet.final)return ESP_OK;
        if(inboundBytes<HeaderBytes){cancelInbound();return ESP_FAIL;}
        const uint32_t sequence=uint32_t(data[12])|uint32_t(data[13])<<8|uint32_t(data[14])<<16|uint32_t(data[15])<<24;
        submitFrame(inboundSlot,inboundBytes);inboundSlot=-1;inboundBytes=0;
        return sendText(req,stateJson("ack",int(sequence)));
    }
    if(packet.type==HTTPD_WS_TYPE_PING || packet.type==HTTPD_WS_TYPE_PONG) {
        uint8_t control[125];if(packet.len>sizeof(control))return ESP_FAIL;
        packet.payload=control;
        if(packet.len && httpd_ws_recv_frame(req,&packet,sizeof(control))!=ESP_OK)return ESP_FAIL;
        if(packet.type==HTTPD_WS_TYPE_PING){packet.type=HTTPD_WS_TYPE_PONG;return httpd_ws_send_frame(req,&packet);}
        return ESP_OK;
    }
    if(packet.type==HTTPD_WS_TYPE_TEXT) {
        if(inboundSlot>=0 || textInProgress){cancelInbound();return ESP_FAIL;}
        textInProgress=true;textBytes=0;
    } else if(packet.type!=HTTPD_WS_TYPE_CONTINUE || !textInProgress)return ESP_FAIL;
    if(packet.len>sizeof(textParts)-1-textBytes){cancelInbound();return ESP_FAIL;}
    packet.payload=reinterpret_cast<uint8_t*>(textParts+textBytes);
    if(packet.len && httpd_ws_recv_frame(req,&packet,sizeof(textParts)-1-textBytes)!=ESP_OK){cancelInbound();return ESP_FAIL;}
    textBytes+=packet.len;if(!packet.final)return ESP_OK;
    textParts[textBytes]=0;textInProgress=false;textBytes=0;
    cJSON* json=cJSON_Parse(textParts);
    if(!json) return sendText(req,"{\"type\":\"error\",\"message\":\"Invalid JSON\"}");
    auto* type=cJSON_GetObjectItemCaseSensitive(json,"type");
    const char* command=cJSON_IsString(type)?type->valuestring:"";
    String error;
    if(strcmp(command,"config")==0) {
        auto o=options();
        auto* item=cJSON_GetObjectItemCaseSensitive(json,"format");if(cJSON_IsNumber(item)) o.format=static_cast<Format>(item->valueint);
        item=cJSON_GetObjectItemCaseSensitive(json,"fps");if(cJSON_IsNumber(item)&&item->valueint>=0&&item->valueint<=50)o.fps=item->valueint;
        item=cJSON_GetObjectItemCaseSensitive(json,"quality");if(cJSON_IsNumber(item)&&item->valueint>=40&&item->valueint<=95)o.quality=item->valueint;
        item=cJSON_GetObjectItemCaseSensitive(json,"fill");if(cJSON_IsBool(item))o.fill=cJSON_IsTrue(item);
        item=cJSON_GetObjectItemCaseSensitive(json,"showFps");if(cJSON_IsBool(item))o.showFps=cJSON_IsTrue(item);
        setOptions(o);
    } else if(strcmp(command,"wifi")==0) {
        auto* ssid=cJSON_GetObjectItemCaseSensitive(json,"ssid");auto* password=cJSON_GetObjectItemCaseSensitive(json,"password");
        if(!cJSON_IsString(ssid)||!cJSON_IsString(password)||
            (strlen(password->valuestring)>0 && strlen(password->valuestring)<8) || !queueWifi(ssid->valuestring,password->valuestring))
            error="SSID must be 1..32 bytes; password empty or 8..64 bytes";
    } else if(strcmp(command,"stop")==0) requestStop();
    else if(strcmp(command,"state")!=0) error="Unknown command";
    cJSON_Delete(json);
    if(!error.isEmpty()) return sendText(req,String("{\"type\":\"error\",\"message\":\"")+error+"\"}");
    return sendText(req,stateJson());
}
esp_err_t ws(httpd_req_t* req) {
    // HTTP and HTTPS run in separate tasks but share one sender and frame assembler.
    if(!wsMutex || xSemaphoreTake(wsMutex,portMAX_DELAY)!=pdTRUE)return ESP_FAIL;
    const esp_err_t result=wsLocked(req);
    xSemaphoreGive(wsMutex);
    return result;
}
bool route(httpd_handle_t server,const char* path,esp_err_t(*handler)(httpd_req_t*),bool websocket=false) {
    httpd_uri_t uri={};uri.uri=path;uri.method=HTTP_GET;uri.handler=handler;
    uri.is_websocket=websocket;uri.handle_ws_control_frames=websocket;
    return httpd_register_uri_handler(server,&uri)==ESP_OK;
}
}
bool serverBegin() {
    if(secureServer && plainServer) return true;
    if(!wsMutex)wsMutex=xSemaphoreCreateMutex();
    if(!wsMutex)return false;
    httpd_ssl_config_t tls=HTTPD_SSL_CONFIG_DEFAULT();
    tls.httpd.stack_size=12288;tls.httpd.max_open_sockets=2;tls.httpd.max_uri_handlers=6;
    tls.httpd.recv_wait_timeout=3;tls.httpd.send_wait_timeout=3;
    tls.cacert_pem=reinterpret_cast<const uint8_t*>(CastCertificate);tls.cacert_len=sizeof(CastCertificate);
    tls.prvtkey_pem=reinterpret_cast<const uint8_t*>(CastPrivateKey);tls.prvtkey_len=sizeof(CastPrivateKey);
    if(httpd_ssl_start(&secureServer,&tls)!=ESP_OK) {secureServer=nullptr;return false;}
    const bool secureOkay=route(secureServer,"/",browserPage)&&route(secureServer,"/ws",ws,true)&&
        route(secureServer,"/api/state",stateGet)&&route(secureServer,"/root.crt",rootCertificate)&&route(secureServer,"/favicon.ico",favicon);
    httpd_config_t plain=HTTPD_DEFAULT_CONFIG();plain.server_port=80;plain.ctrl_port=32769;plain.max_open_sockets=2;
    plain.stack_size=4096;plain.max_uri_handlers=5;plain.lru_purge_enable=true;
    if(!secureOkay || httpd_start(&plainServer,&plain)!=ESP_OK ||
       !route(plainServer,"/",setupPage)||!route(plainServer,"/root.crt",rootCertificate)||!route(plainServer,"/favicon.ico",favicon)||
       !route(plainServer,"/ws",ws,true)||!route(plainServer,"/api/state",stateGet)) {
        if(plainServer)httpd_stop(plainServer);plainServer=nullptr;
        httpd_ssl_stop(secureServer);secureServer=nullptr;return false;
    }
    return true;
}
}
