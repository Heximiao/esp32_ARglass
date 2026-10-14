#include "St7789.h"
#include <esp_heap_caps.h>

esp_err_t St7789::command(uint8_t cmd,const uint8_t* data,size_t len) {
    digitalWrite(cfg_.dc,LOW);
    spi_transaction_t t={};
    t.flags=SPI_TRANS_USE_TXDATA; t.length=8; t.tx_data[0]=cmd;
    esp_err_t err=spi_device_polling_transmit(dev_,&t);
    if(err!=ESP_OK || !len) return err;
    digitalWrite(cfg_.dc,HIGH);
    t={}; t.length=len*8; t.tx_buffer=data;
    return spi_device_polling_transmit(dev_,&t);
}

esp_err_t St7789::begin(const Config& cfg) {
    if(owned_) return ESP_ERR_INVALID_STATE;
    if(cfg.xOffset>320-Width || cfg.yOffset>240-Height ||
       cfg.clockHz<=0 || cfg.clockHz>80000000 ||
       cfg.backlightPwmChannel<0 || cfg.backlightPwmChannel>15)
        return ESP_ERR_INVALID_ARG;
    cfg_=cfg;
    pinMode(cfg_.bl,OUTPUT); setBacklight(false);
    pinMode(cfg_.dc,OUTPUT); pinMode(cfg_.res,OUTPUT);
    spi_bus_config_t bus={};
    bus.sclk_io_num=cfg_.scl; bus.mosi_io_num=cfg_.sda;
    bus.miso_io_num=-1; bus.quadwp_io_num=-1; bus.quadhd_io_num=-1;
    bus.max_transfer_sz=ChunkPixels*2;
    esp_err_t err=spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO);
    if(err!=ESP_OK) return err;
    owned_=true;
    spi_device_interface_config_t device={};
    device.clock_speed_hz=cfg_.clockHz; device.mode=0;
    device.spics_io_num=cfg_.cs; device.queue_size=2;
    device.flags=SPI_DEVICE_HALFDUPLEX;
    err=spi_bus_add_device(SPI2_HOST,&device,&dev_);
    if(err!=ESP_OK) { end(); return err; }
    for(auto& b:buf_) {
        b=static_cast<uint8_t*>(heap_caps_malloc(ChunkPixels*2,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));
        if(!b) { end(); return ESP_ERR_NO_MEM; }
    }
    digitalWrite(cfg_.res,HIGH); delay(10);
    digitalWrite(cfg_.res,LOW); delay(20);
    digitalWrite(cfg_.res,HIGH); delay(120);
    auto send=[&](uint8_t cmd,const uint8_t* data=nullptr,size_t len=0) {
        if(err==ESP_OK) err=command(cmd,data,len);
    };
    send(0x01); delay(150);
    send(0x11); delay(120);
    const uint8_t format[]={0x55}; send(0x3A,format,sizeof(format));
    // MV swaps axes; MX sets the landscape scan direction (90-degree turn).
    const uint8_t madctl[]={uint8_t(0x60 | (cfg_.bgr?0x08:0x00))}; send(0x36,madctl,sizeof(madctl));
    // Baseline ST7789 analog settings; exact supplier init may differ.
    const uint8_t porch[]={0x0C,0x0C,0x00,0x33,0x33}; send(0xB2,porch,sizeof(porch));
    const uint8_t gate[]={0x35}; send(0xB7,gate,sizeof(gate));
    const uint8_t vcom[]={0x19}; send(0xBB,vcom,sizeof(vcom));
    const uint8_t lcm[]={0x2C}; send(0xC0,lcm,sizeof(lcm));
    const uint8_t enable[]={0x01}; send(0xC2,enable,sizeof(enable));
    const uint8_t vrh[]={0x12}; send(0xC3,vrh,sizeof(vrh));
    const uint8_t vdv[]={0x20}; send(0xC4,vdv,sizeof(vdv));
    const uint8_t rate[]={0x0F}; send(0xC6,rate,sizeof(rate));
    const uint8_t power[]={0xA4,0xA1}; send(0xD0,power,sizeof(power));
    send(cfg_.inverted?0x21:0x20); send(0x13); send(0x29); delay(20);
    if(err!=ESP_OK) { end(); return err; }
    ready_=true;
    err=fill(0);
    if(err!=ESP_OK) { end(); return err; }
    if(ledcSetup(cfg_.backlightPwmChannel,20000,8)==0) { end(); return ESP_FAIL; }
    ledcAttachPin(cfg_.bl,cfg_.backlightPwmChannel);
    backlightPwmReady_=true;
    setBacklight(true);
    return ESP_OK;
}

void St7789::setBacklight(bool on) {
    if(backlightPwmReady_) {
        const uint32_t duty=on?uint32_t(brightness_)*255/100:0;
        ledcWrite(cfg_.backlightPwmChannel,cfg_.backlightActiveHigh?duty:255-duty);
    } else digitalWrite(cfg_.bl,on==cfg_.backlightActiveHigh?HIGH:LOW);
}
void St7789::setBrightness(uint8_t percent) {
    brightness_=percent>100?100:percent;
    if(ready_) setBacklight(true);
}
void St7789::end() {
    if(owned_) setBacklight(false);
    if(backlightPwmReady_) { ledcDetachPin(cfg_.bl); backlightPwmReady_=false; }
    ready_=false;
    if(dev_) { spi_bus_remove_device(dev_); dev_=nullptr; }
    if(owned_) { spi_bus_free(SPI2_HOST); owned_=false; }
    for(auto& b:buf_) { heap_caps_free(b); b=nullptr; }
}
esp_err_t St7789::window(uint16_t x,uint16_t y,uint16_t w,uint16_t h) {
    const uint16_t x0=x+cfg_.xOffset,x1=x0+w-1;
    const uint16_t y0=y+cfg_.yOffset,y1=y0+h-1;
    const uint8_t col[]={uint8_t(x0>>8),uint8_t(x0),uint8_t(x1>>8),uint8_t(x1)};
    const uint8_t row[]={uint8_t(y0>>8),uint8_t(y0),uint8_t(y1>>8),uint8_t(y1)};
    esp_err_t err=command(0x2A,col,sizeof(col));
    if(err==ESP_OK) err=command(0x2B,row,sizeof(row));
    if(err==ESP_OK) err=command(0x2C);
    return err;
}
esp_err_t St7789::transfer(const uint16_t* pixels,size_t count,uint16_t color) {
    digitalWrite(cfg_.dc,HIGH);
    spi_transaction_t trans[2]={};
    size_t queued=0,completed=0,offset=0;
    esp_err_t err=ESP_OK;
    while(offset<count) {
        const size_t slot=queued%2;
        if(queued-completed==2) {
            spi_transaction_t* done=nullptr;
            err=spi_device_get_trans_result(dev_,&done,portMAX_DELAY);
            if(err!=ESP_OK) break;
            ++completed;
        }
        const size_t n=count-offset<ChunkPixels?count-offset:ChunkPixels;
        for(size_t i=0;i<n;++i) {
            const uint16_t value=pixels?pixels[offset+i]:color;
            buf_[slot][2*i]=value>>8; buf_[slot][2*i+1]=value&0xFF;
        }
        auto& t=trans[slot]; t={}; t.length=n*16; t.tx_buffer=buf_[slot];
        err=spi_device_queue_trans(dev_,&t,portMAX_DELAY);
        if(err!=ESP_OK) break;
        ++queued; offset+=n;
    }
    // Drain before stack descriptors or source storage can be reused.
    while(completed<queued) {
        spi_transaction_t* done=nullptr;
        const esp_err_t result=spi_device_get_trans_result(dev_,&done,portMAX_DELAY);
        if(result!=ESP_OK) { if(err==ESP_OK) err=result; break; }
        ++completed;
    }
    return err;
}
esp_err_t St7789::drawRect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t* pixels) {
    if(!ready_) return ESP_ERR_INVALID_STATE;
    if(!pixels || !w || !h || x>=Width || y>=Height || w>Width-x || h>Height-y)
        return ESP_ERR_INVALID_ARG;
    esp_err_t err=window(x,y,w,h);
    return err==ESP_OK?transfer(pixels,size_t(w)*h,0):err;
}
esp_err_t St7789::fill(uint16_t color) {
    if(!ready_) return ESP_ERR_INVALID_STATE;
    esp_err_t err=window(0,0,Width,Height);
    return err==ESP_OK?transfer(nullptr,size_t(Width)*Height,color):err;
}
