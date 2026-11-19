#include "bk_ui.h"
#include "bk_screens.h"
#include "bk_widgets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static struct {
 lv_obj_t *root,*body,*status,*value,*unit,*left,*right,*arc;
 double values[BK_METRIC_COUNT]; bk_value_state_t states[BK_METRIC_COUNT];
 unsigned screen; bool dark, connected; bk_metric_t selected[5];
 char ssid[33], password[65], url[96];
} ui;
static lv_color_t fg(void){return lv_color_hex(ui.dark?0xe9f1ed:0x112b20);}
static lv_color_t accent(void){return lv_color_hex(ui.dark?0x72d6aa:0x126b48);}
static void show(void);
static void format_metric(bk_metric_t m,char *out,size_t size,bool with_name){
 const char *state="--";
 if(ui.states[m]==BK_VALUE_VALID && isfinite(ui.values[m])) {
  unsigned precision=(m==BK_VOLTAGE||m==BK_FUEL_LPH||m==BK_INJECTION)?1:0;
  if(with_name) snprintf(out,size,"%s\n%.*f %s",bk_metric_names[m],precision,ui.values[m],bk_metric_units[m]);
  else snprintf(out,size,"%.*f",precision,ui.values[m]);
  return;
 }
 switch(ui.states[m]) {case BK_VALUE_STALE:state="Устарело";break;
 case BK_VALUE_UNSUPPORTED:state="Недоступно";break;
 case BK_VALUE_INSUFFICIENT:state="Нет данных";break;default:break;}
 if(with_name)snprintf(out,size,"%s\n%s",bk_metric_names[m],state);else snprintf(out,size,"%s",state);
}
static void refresh(void){
 if(!ui.root)return;
 lv_label_set_text(ui.status,ui.connected?"ЭБУ: подключён":"ЭБУ: нет связи");
 if(ui.screen<5 && ui.value){
  bk_metric_t m=ui.selected[ui.screen]; char buf[128];
  format_metric(m,buf,sizeof(buf),false);lv_label_set_text(ui.value,buf);
  lv_obj_set_style_text_font(ui.value,ui.states[m]==BK_VALUE_VALID?&lv_font_montserrat_48:&bk_font_ru_20,0);
  lv_label_set_text(ui.unit,bk_metric_units[m]);
  format_metric(bk_screens[ui.screen].left,buf,sizeof(buf),true);lv_label_set_text(ui.left,buf);
  format_metric(bk_screens[ui.screen].right,buf,sizeof(buf),true);lv_label_set_text(ui.right,buf);
  int n=ui.states[m]==BK_VALUE_VALID?(int)(ui.values[m]/bk_metric_max[m]*1000):0;
  lv_arc_set_value(ui.arc,n<0?0:n>1000?1000:n);
  bool warning=ui.states[m]==BK_VALUE_VALID&&((m==BK_COOLANT&&ui.values[m]>=105)||(m==BK_VOLTAGE&&ui.values[m]<12)||(m==BK_RPM&&ui.values[m]>=5800));
  lv_obj_set_style_text_color(ui.value,warning?lv_color_hex(0xc74732):fg(),0);
 } else if(ui.screen==7 && ui.value){char buf[80];format_metric(BK_DTC_COUNT,buf,sizeof(buf),false);lv_label_set_text(ui.value,buf);}
}
static void navigate(lv_event_t *event){unsigned n=(unsigned)(uintptr_t)lv_event_get_user_data(event);bk_ui_select_screen(n);}
static void theme_event(lv_event_t *event){(void)event;bk_ui_set_theme(!ui.dark);}
static void value_event(lv_event_t *event){(void)event;if(ui.screen<5){ui.selected[ui.screen]=(ui.selected[ui.screen]+1)%BK_METRIC_COUNT;show();}}
static void gesture(lv_event_t *event){(void)event;lv_dir_t d=lv_indev_get_gesture_dir(lv_indev_active());
 if(d==LV_DIR_LEFT)bk_ui_select_screen((ui.screen+1)%5);else if(d==LV_DIR_RIGHT)bk_ui_select_screen((ui.screen+4)%5);}
static lv_obj_t *button(const char *label,int x,int y,int w,unsigned target){
 lv_obj_t *b=lv_button_create(ui.body);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,44);
 lv_obj_set_style_bg_color(b,accent(),0);lv_obj_set_style_radius(b,16,0);
 lv_obj_t *t=bk_text(b,label,0,0,w-16);lv_obj_center(t);lv_obj_set_style_text_color(t,lv_color_hex(ui.dark?0x112b20:0xffffff),0);
 lv_obj_add_event_cb(b,navigate,LV_EVENT_CLICKED,(void *)(uintptr_t)target);return b;
}
static void show(void){
 if(!ui.root)return;
 lv_obj_clean(ui.body);ui.value=ui.unit=ui.left=ui.right=ui.arc=NULL;
 lv_obj_set_style_bg_color(ui.root,lv_color_hex(ui.dark?0x101c17:0xf7f7ef),0);
 lv_obj_set_style_text_color(ui.root,fg(),0);
 if(ui.screen<5){
  ui.arc=bk_gauge(ui.body,accent());
  bk_text(ui.body,bk_screens[ui.screen].title,95,94,290);
  bk_metric_t m=ui.selected[ui.screen];bk_text(ui.body,bk_metric_names[m],90,151,300);
  ui.value=bk_text(ui.body,"--",75,192,330);
  lv_obj_add_flag(ui.value,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(ui.value,value_event,LV_EVENT_CLICKED,NULL);
  ui.unit=bk_text(ui.body,"",130,252,220);
  ui.left=bk_text(ui.body,"",68,309,166);ui.right=bk_text(ui.body,"",246,309,166);
  button("Меню",180,381,120,5);
 }else if(ui.screen==5){
  bk_text(ui.body,"HUNTER · МЕНЮ",80,65,320);
  button("Приборы",115,113,250,0);button("Wi-Fi",115,167,250,6);
  button("Ошибки ЭБУ",115,221,250,7);button("Настройки",115,275,250,8);
  button("О приборе",140,329,200,9);
 }else if(ui.screen==6){
  bk_text(ui.body,"ПОДКЛЮЧЕНИЕ WI-FI",60,65,360);
  if(ui.ssid[0]){
   char data[240];/* WIFI QR grammar: escape delimiters before publishing. */
   char ss[67],pw[131];size_t s=0,p=0;
   for(size_t i=0;ui.ssid[i];i++){if(strchr("\\;,:\"",ui.ssid[i]))ss[s++]='\\';ss[s++]=ui.ssid[i];}ss[s]=0;
   for(size_t i=0;ui.password[i];i++){if(strchr("\\;,:\"",ui.password[i]))pw[p++]='\\';pw[p++]=ui.password[i];}pw[p]=0;
   snprintf(data,sizeof(data),"WIFI:T:WPA;S:%s;P:%s;;",ss,pw);
   lv_obj_t *qr=lv_qrcode_create(ui.body);lv_qrcode_set_size(qr,180);
   lv_qrcode_set_dark_color(qr,lv_color_hex(0x000000));lv_qrcode_set_light_color(qr,lv_color_hex(0xffffff));
   lv_obj_set_pos(qr,150,119);lv_qrcode_update(qr,data,strlen(data));
   bk_text(ui.body,ui.ssid,80,311,320);bk_text(ui.body,ui.url,70,341,340);
  }else bk_text(ui.body,"Точка доступа выключена\nQR появится после запуска AP",70,185,340);
  button("Назад",180,381,120,5);
 }else if(ui.screen==7){
  bk_text(ui.body,"ОШИБКИ ЭБУ",100,92,280);ui.value=bk_text(ui.body,"--",100,172,280);
  bk_text(ui.body,"Количество подтверждённых DTC\nТолько чтение",70,238,340);button("Назад",180,351,120,5);
 }else if(ui.screen==8){
  bk_text(ui.body,"НАСТРОЙКИ",100,92,280);
  lv_obj_t *b=button(ui.dark?"Тема: тёмная":"Тема: светлая",115,165,250,8);
  lv_obj_remove_event_cb(b,navigate);lv_obj_add_event_cb(b,theme_event,LV_EVENT_CLICKED,NULL);
  bk_text(ui.body,"Нажмите на большое значение,\nчтобы выбрать другой параметр.\nСвайп: следующий прибор.",60,235,360);
  button("Назад",180,351,120,5);
 }else{
  bk_text(ui.body,"УАЗ",100,100,280);
  lv_obj_t *title=bk_text(ui.body,"HUNTER",70,170,340);lv_obj_set_style_text_font(title,&lv_font_montserrat_48,0);
  bk_text(ui.body,"БОРТОВОЙ КОМПЬЮТЕР\nМИКАС 7.2",70,250,340);button("Начать",165,351,150,0);
 }
 refresh();
}
void bk_ui_create(lv_obj_t *parent){
 memset(&ui,0,sizeof(ui));for(unsigned i=0;i<5;i++)ui.selected[i]=bk_screens[i].primary;
 ui.root=lv_obj_create(parent);lv_obj_set_size(ui.root,480,480);lv_obj_center(ui.root);
 lv_obj_remove_flag(ui.root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(ui.root,0,0);
 lv_obj_set_style_border_width(ui.root,0,0);lv_obj_set_style_radius(ui.root,240,0);
 lv_obj_set_style_clip_corner(ui.root,true,0);
 ui.body=lv_obj_create(ui.root);lv_obj_remove_style_all(ui.body);
 lv_obj_set_size(ui.body,480,480);lv_obj_set_pos(ui.body,0,0);lv_obj_remove_flag(ui.body,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_add_event_cb(ui.body,gesture,LV_EVENT_GESTURE,NULL);
 ui.status=bk_text(ui.root,"",100,438,280);ui.screen=1;show();
}
void bk_ui_set_sample(bk_metric_t m,double value,bk_value_state_t state){
 if((unsigned)m>=BK_METRIC_COUNT)return;
 bk_value_state_t next=isfinite(value)?state:BK_VALUE_UNKNOWN;
 if(ui.states[m]==next && (ui.values[m]==value || (!isfinite(ui.values[m])&&!isfinite(value))))return;
 ui.values[m]=value;ui.states[m]=next;refresh();
}
void bk_ui_set_connected(bool connected){if(ui.connected==connected)return;ui.connected=connected;refresh();}
void bk_ui_set_theme(bool dark){ui.dark=dark;show();}
void bk_ui_select_screen(unsigned n){if(n>=10)return;ui.screen=n;show();}
unsigned bk_ui_screen(void){return ui.screen;}
void bk_ui_set_wifi(const char *ssid,const char *password,const char *url){
 snprintf(ui.ssid,sizeof(ui.ssid),"%s",ssid?ssid:"");snprintf(ui.password,sizeof(ui.password),"%s",password?password:"");
 snprintf(ui.url,sizeof(ui.url),"%s",url?url:"");if(ui.screen==6)show();
}

void bk_ui_get_config(bk_ui_config_t *out){
 if(!out)return;
 out->screen=ui.screen;out->dark=ui.dark;
 for(unsigned i=0;i<5;i++)out->selected[i]=ui.selected[i];
}
bool bk_ui_set_config(const bk_ui_config_t *config){
 if(!config||config->screen>=10)return false;
 for(unsigned i=0;i<5;i++)if((unsigned)config->selected[i]>=BK_METRIC_COUNT)return false;
 ui.screen=config->screen;ui.dark=config->dark;
 for(unsigned i=0;i<5;i++)ui.selected[i]=config->selected[i];
 show();return true;
}
