#include "PasswordKeyboard.h"
#include "Widgets.h"
#include <cstring>
namespace ui { namespace keyboard {
namespace {
constexpr char sets[3][41]={"abcdefghijklmnopqrstuvwxyz0123456789-_.@",
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.@",
                    "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~01234567"};
const char* actions[]={"ABC","#+=","SP","DEL","EYE","JOIN"};
lv_obj_t* cells[46];lv_obj_t* captions[46];lv_obj_t* password;lv_obj_t* notice;
String text;unsigned mode=0,row=0,col=0;bool visible=false;
unsigned index(){return row<4?row*10+col:40+col;}
void refresh(){
    String shown;
    if(visible)shown=text;else for(unsigned i=0;i<text.length();++i)shown+='*';
    if(shown.isEmpty())shown="...";
    lv_label_set_text(password,shown.c_str());
    for(unsigned i=0;i<40;++i){char key[]={sets[mode][i],0};lv_label_set_text(captions[i],key);}
    lv_label_set_text(captions[40],mode==1?"abc":"ABC");
    for(unsigned i=0;i<46;++i){focus(cells[i],i==index());lv_obj_set_style_border_width(cells[i],1,0);}
}
}
void create(lv_obj_t* root,const char* ssid){
    clear();mode=0;row=col=0;visible=false;
    header(root,"","PASSWORD");
    auto* name=notice=label(root,ssid,12,9,Muted);lv_obj_set_width(name,viewport::ContentWidth-80);lv_label_set_long_mode(name,LV_LABEL_LONG_SCROLL_CIRCULAR);
    auto* field=panel(root,12,35,viewport::ContentWidth,23);
    password=label(field,"",5,1,Accent,&lv_font_montserrat_14);
    lv_obj_set_width(password,viewport::ContentWidth-10);lv_label_set_long_mode(password,LV_LABEL_LONG_SCROLL_CIRCULAR);
    for(unsigned i=0;i<46;++i){
        const bool action=i>=40;unsigned x=action?i-40:i%10,y=action?4:i/10;
        const int pitch=action?38:23;
        cells[i]=panel(root,12+x*pitch,60+y*18,pitch-1,17);
        lv_obj_set_style_radius(cells[i],3,0);
        captions[i]=label(cells[i],action?actions[i-40]:"",0,0,White,&lv_font_montserrat_12);
        lv_obj_center(captions[i]);
    }
    refresh();
}
bool input(Input event){
    if(event==Input::Up){row=(row+4)%5;col=col%(row==4?6:10);}
    if(event==Input::Down){row=(row+1)%5;col=col%(row==4?6:10);}
    if(event==Input::Left)col=(col+(row==4?5:9))%(row==4?6:10);
    if(event==Input::Right)col=(col+1)%(row==4?6:10);
    if(event==Input::Confirm){
        const unsigned i=index();
        if(i<40 && text.length()<64)text+=sets[mode][i];
        if(i==40)mode=mode==1?0:1;
        if(i==41)mode=mode==2?0:2;
        if(i==42 && text.length()<64)text+=' ';
        if(i==43 && text.length())text.remove(text.length()-1);
        if(i==44)visible=!visible;
        if(i==45)return true;
    }
    refresh();return false;
}
const String& value(){return text;}
void clear(){text="";}
void error(const char* message){lv_label_set_text(notice,message);lv_obj_set_style_text_color(notice,lv_color_hex(0xFF8D83),0);}
} }
