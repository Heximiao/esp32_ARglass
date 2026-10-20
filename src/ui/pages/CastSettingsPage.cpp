#include "CastSettingsPage.h"
#include "../Widgets.h"
#include "../../cast/CastService.h"

namespace ui { namespace castsettings {
namespace {
lv_obj_t* rows[5];lv_obj_t* values[5];lv_obj_t* list;
unsigned selected=0;Page parent=Page::Cast;
const uint8_t Rates[]={5,10,15,20,25,30,40,50,0};
void refresh(lv_anim_enable_t animate=LV_ANIM_ON) {
    const auto o=casting::options();
    lv_label_set_text(values[0],o.format==casting::Format::Jpeg?"JPEG":"RGB565");
    if(o.fps)lv_label_set_text_fmt(values[1],"%u FPS",o.fps);else lv_label_set_text(values[1],"AUTO / 50");
    lv_label_set_text_fmt(values[2],"%u / 100",o.quality);
    lv_label_set_text(values[3],o.fill?"CROP":"FIT");
    lv_label_set_text(values[4],o.showFps?"ON":"OFF");
    for(unsigned i=0;i<5;++i){focus(rows[i],i==selected);lv_obj_align(values[i],LV_ALIGN_RIGHT_MID,-12,0);}
    lv_obj_update_layout(list);lv_obj_scroll_to_view(rows[selected],animate);
}
}
lv_obj_t* create(Page returnTo) {
    parent=returnTo==Page::Settings?Page::Settings:Page::Cast;
    auto* root=pageRoot();header(root,"投屏设置","VIDEO ONLY");
    list=scrollArea(root,12,35,viewport::ContentWidth,110,LV_DIR_VER);
    const char* names[]={"传输格式","目标帧率","图片质量","画面比例","显示帧率"};
    for(unsigned i=0;i<5;++i){rows[i]=panel(list,0,i*36,viewport::ContentWidth,32);
        label(rows[i],names[i],12,4);values[i]=label(rows[i],"",0,0,Accent,&lv_font_montserrat_12);}
    refresh(LV_ANIM_OFF);footer(root,"U/D select  L/R change  HOLD back");return root;
}
void input(Input event) {
    if(event==Input::Back){navigate(parent);return;}
    if(event==Input::Up)selected=(selected+4)%5;
    if(event==Input::Down)selected=(selected+1)%5;
    if(event==Input::Left||event==Input::Right||event==Input::Confirm){
        auto o=casting::options();const bool previous=event==Input::Left;
        if(selected==0)o.format=o.format==casting::Format::Jpeg?casting::Format::Rgb565:casting::Format::Jpeg;
        if(selected==1){unsigned index=0;for(unsigned i=0;i<9;++i)if(Rates[i]==o.fps)index=i;
            o.fps=Rates[(index+(previous?8:1))%9];}
        if(selected==2){const uint8_t qualities[]={50,70,90};unsigned index=o.quality<=50?0:o.quality<=70?1:2;
            o.quality=qualities[(index+(previous?2:1))%3];}
        if(selected==3)o.fill=!o.fill;if(selected==4)o.showFps=!o.showFps;
        casting::setOptions(o);
    }
    refresh();
}
} }
