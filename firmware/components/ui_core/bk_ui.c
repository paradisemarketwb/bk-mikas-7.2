#include "bk_ui.h"
#include "bk_screens.h"
#include "bk_widgets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DTC_LIMIT 127
static struct {
 lv_obj_t *root,*body,*status,*editor_error,*boot_icon,*brightness_label;
 bk_layout_t layout;
 bk_widget_t widgets[BK_LAYOUT_MAX_WIDGETS];unsigned rendered;
 double values[BK_METRIC_COUNT];bk_value_state_t states[BK_METRIC_COUNT];
 unsigned screen,last_instrument;bool dark,connected;uint8_t brightness;
 char ssid[33],password[65],url[96],wifi_status[64];
 uint8_t dtcs[DTC_LIMIT];size_t dtc_count;bool dtcs_valid;
 const char *(*dtc_lookup)(uint8_t);
 void (*brightness_callback)(uint8_t,void *);void *brightness_context;
 lv_timer_t *history_timer,*boot_timer;unsigned boot_target;
 unsigned edit_mode,edit_screen,edit_widget;bool delete_pending;bk_widget_config_t draft;
 lv_obj_t *edit_values[17];
} ui;
static void show(void);
static void show_editor(void);
static lv_color_t fg(void){return lv_color_hex(ui.dark?0xe9f1ed:0x112b20);}
static lv_color_t accent(void){return lv_color_hex(ui.dark?0x72d6aa:0x126b48);}
static void label(lv_obj_t *o,const char *s){if(o&&strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}
static bool instrument(unsigned view){int s=bk_layout_view_to_screen(view);return s>=0&&(unsigned)s<ui.layout.screen_count&&ui.layout.screens[s].enabled;}
static void refresh(void){
 if(!ui.root)return;
 label(ui.status,ui.connected?"ЭБУ: подключён":"ЭБУ: нет связи");
 for(unsigned i=0;i<ui.rendered;i++){
  bk_widget_t *w=&ui.widgets[i];bk_metric_t m=w->config.metric;
  bk_widget_update(w,ui.values[m],ui.states[m],ui.connected,bk_metric_names[m],bk_metric_units[m]);
 }
}
static void navigate(lv_event_t *e){bk_ui_select_screen((unsigned)(uintptr_t)lv_event_get_user_data(e));}
static lv_obj_t *button_on(lv_obj_t *parent,const char *s,int x,int y,int width,lv_event_cb_t cb,uintptr_t value){
 lv_obj_t *o=lv_button_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,44);
 lv_obj_set_style_bg_color(o,accent(),0);lv_obj_set_style_radius(o,14,0);
 lv_obj_t *t=bk_text(o,s,0,0,width-12);lv_obj_center(t);lv_obj_set_style_text_color(t,lv_color_hex(ui.dark?0x112b20:0xffffff),0);
 if(cb)lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,(void *)value);
 return o;
}
static lv_obj_t *single_line(const char *text,int x,int y,int width){
 lv_obj_t *o=bk_text(ui.body,text,x,y,width);lv_label_set_long_mode(o,LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);lv_obj_set_height(o,26);return o;
}
static lv_obj_t *button(const char *s,int x,int y,int width,unsigned target){return button_on(ui.body,s,x,y,width,navigate,target);}
static void theme_event(lv_event_t *e){(void)e;bk_ui_set_theme(!ui.dark);}
static void next_instrument(int direction){
 int current=bk_layout_view_to_screen(ui.screen);if(current<0)current=(int)ui.last_instrument;
 for(unsigned i=0;i<ui.layout.screen_count;i++){
  current=(current+direction+(int)ui.layout.screen_count)%(int)ui.layout.screen_count;
  if(ui.layout.screens[current].enabled){bk_ui_select_screen(bk_layout_screen_to_view(current));return;}
 }
}
static void gesture(lv_event_t *e){(void)e;if(ui.edit_mode)return;lv_indev_t *input=lv_indev_active();if(!input)return;
 lv_dir_t d=lv_indev_get_gesture_dir(input);if(d==LV_DIR_LEFT)next_instrument(1);else if(d==LV_DIR_RIGHT)next_instrument(-1);
}
static void widget_pressed(lv_event_t *e){
 unsigned n=(unsigned)(uintptr_t)lv_event_get_user_data(e);
 if(n>=ui.rendered)return;
 int screen=bk_layout_view_to_screen(ui.screen);if(screen<0)return;
 ui.edit_screen=(unsigned)screen;ui.edit_widget=n;ui.draft=ui.layout.screens[screen].widgets[n];ui.edit_mode=2;show_editor();
}
static void editor_list(lv_event_t *e){(void)e;ui.edit_screen=ui.last_instrument;ui.edit_mode=1;show_editor();}
static lv_obj_t *scroll_panel(void){
 lv_obj_t *p=lv_obj_create(ui.body);lv_obj_set_pos(p,75,113);lv_obj_set_size(p,330,247);
 lv_obj_set_style_text_color(p,fg(),0);
 lv_obj_set_style_bg_color(p,lv_color_hex(ui.dark?0x172b20:0xf7f7ef),0);lv_obj_set_style_border_width(p,0,0);
 lv_obj_set_style_pad_all(p,6,0);lv_obj_set_scroll_dir(p,LV_DIR_VER);return p;
}
static void edit_pick(lv_event_t *e){
 unsigned n=(unsigned)(uintptr_t)lv_event_get_user_data(e);bk_layout_screen_t *s=&ui.layout.screens[ui.edit_screen];
 ui.edit_widget=n;ui.draft=n<s->widget_count?s->widgets[n]:(bk_widget_config_t){.type=BK_WIDGET_NUMBER,.metric=BK_RPM,.x=100,.y=150,.w=280,.h=130,.min=0,.max=6000,.color=0x126b48,.visible=true,.font_size=48};ui.edit_mode=2;show_editor();
}
static void editor_exit(lv_event_t *e){(void)e;ui.edit_mode=0;show();}
static void editor_back(lv_event_t *e){(void)e;ui.edit_mode=1;show_editor();}
static void edit_screen_change(lv_event_t *e){(void)e;ui.edit_screen=(ui.edit_screen+1)%ui.layout.screen_count;show_editor();}
static void edit_delete(lv_event_t *e){
 if(!ui.delete_pending){ui.delete_pending=true;label(ui.editor_error,"Нажмите ещё раз для удаления");
  lv_obj_t *b=lv_event_get_target_obj(e);label(lv_obj_get_child(b,0),"Подтвердить удаление");lv_obj_set_style_bg_color(b,lv_color_hex(0xc74732),0);return;}
bk_layout_screen_t *s=&ui.layout.screens[ui.edit_screen];if(ui.edit_widget<s->widget_count){
  for(unsigned n=ui.edit_widget+1;n<s->widget_count;n++)s->widgets[n-1]=s->widgets[n];
  s->widget_count--;
 }ui.edit_mode=1;show_editor();
}
static void edit_save(lv_event_t *e){
 (void)e;bk_layout_screen_t *s=&ui.layout.screens[ui.edit_screen];
 bool add=ui.edit_widget==s->widget_count;bk_widget_config_t before=s->widgets[ui.edit_widget];
 s->widgets[ui.edit_widget]=ui.draft;if(add)s->widget_count++;
 if(!bk_layout_validate(&ui.layout)){s->widgets[ui.edit_widget]=before;if(add)s->widget_count--;label(ui.editor_error,"Проверьте размеры и диапазон");return;}
 ui.edit_mode=1;show_editor();
}
static const uint32_t palette[]={0x126b48,0x2484bf,0xc74732,0xb87516,0x8059b4,0x72d6aa};
static void update_editor_labels(void){
 char b[96];bk_widget_config_t *w=&ui.draft;
 snprintf(b,sizeof(b),"Вид: %s",bk_widget_registry[w->type].name);label(ui.edit_values[0],b);
 snprintf(b,sizeof(b),"Параметр: %s",bk_metric_names[w->metric]);label(ui.edit_values[1],b);
 const double v[]={w->x,w->y,w->w,w->h,w->min,w->max,w->warn_low,w->warn_high,w->precision,w->font_size};
 const char *names[]={"X","Y","Ширина","Высота","Мин.","Макс.","Порог низ.","Порог верх.","Точность","Шрифт"};
 for(unsigned i=0;i<10;i++){snprintf(b,sizeof(b),"%s: %.1f",names[i],v[i]);label(ui.edit_values[i+2],b);}
 snprintf(b,sizeof(b),"Цвет: #%06lx",(unsigned long)w->color);label(ui.edit_values[12],b);
 snprintf(b,sizeof(b),"Пороги: %s%s",w->flags&BK_WARN_LOW?"низ ":"",w->flags&BK_WARN_HIGH?"верх":"выкл");label(ui.edit_values[13],b);
 label(ui.edit_values[14],w->visible?"Видимость: да":"Видимость: нет");
 snprintf(b,sizeof(b),"Единицы: %s",w->unit[0]?w->unit:"авто");label(ui.edit_values[15],b);
}
static void edit_step(lv_event_t *e){
 unsigned code=(unsigned)(uintptr_t)lv_event_get_user_data(e),field=code/2;int d=(code%2)?1:-1;bk_widget_config_t *w=&ui.draft;
 switch(field){
 case 0:w->type=(w->type+d+BK_WIDGET_COUNT)%BK_WIDGET_COUNT;break;
 case 1:
  w->metric=(w->metric+d+BK_METRIC_COUNT)%BK_METRIC_COUNT;w->min=0;w->max=bk_metric_max[w->metric];w->unit[0]=0;w->flags=0;
  w->precision=w->metric==BK_VOLTAGE||w->metric==BK_FUEL_LPH||w->metric==BK_INJECTION?1:0;
  if(w->metric==BK_COOLANT){w->flags=BK_WARN_HIGH;w->warn_high=105;}
  if(w->metric==BK_RPM){w->flags=BK_WARN_HIGH;w->warn_high=5800;}
  if(w->metric==BK_VOLTAGE){w->flags=BK_WARN_LOW;w->warn_low=12;}
  break;
 case 2:if(w->x+d*4>=0&&w->x+d*4<=464)w->x+=d*4;break;
 case 3:if(w->y+d*4>=0&&w->y+d*4<=416)w->y+=d*4;break;
 case 4:if(w->w+d*4>=16&&w->w+d*4<=480)w->w+=d*4;break;
 case 5:if(w->h+d*4>=16&&w->h+d*4<=416)w->h+=d*4;break;
 case 6:w->min+=d;break;case 7:w->max+=d;break;
 case 8:w->warn_low+=d;break;case 9:w->warn_high+=d;break;
 case 10:w->precision=(w->precision+d+4)%4;break;case 11:w->font_size=w->font_size==20?48:20;break;
 case 12:{unsigned p=0;while(p<6&&palette[p]!=w->color)p++;w->color=palette[(p+d+6)%6];break;}
 case 13:w->flags=(w->flags+d+4)%4;break;case 14:w->visible=!w->visible;break;
 case 15:if(w->unit[0])w->unit[0]=0;else snprintf(w->unit,sizeof(w->unit),"%s",bk_metric_units[w->metric]);break;
 default:break;
 }update_editor_labels();label(ui.editor_error,"");
}
static void show_editor(void){
 ui.rendered=0;lv_obj_clean(ui.body);ui.editor_error=NULL;ui.delete_pending=false;memset(ui.edit_values,0,sizeof(ui.edit_values));
 bk_text(ui.body,ui.edit_mode==1?"РЕДАКТОР ЭКРАНА":"НАСТРОЙКА ВИДЖЕТА",60,66,360);
 lv_obj_t *panel=scroll_panel();bk_layout_screen_t *s=&ui.layout.screens[ui.edit_screen];
 if(ui.edit_mode==1){
  button_on(panel,s->title,0,0,300,edit_screen_change,0);
  for(unsigned i=0;i<s->widget_count;i++){
   char b[80];snprintf(b,sizeof(b),"%u. %s",i+1,bk_widget_registry[s->widgets[i].type].name);
   button_on(panel,b,0,54+(int)i*52,300,edit_pick,i);
  }
  if(s->widget_count<BK_LAYOUT_MAX_WIDGETS)button_on(panel,"+ Добавить виджет",0,54+(int)s->widget_count*52,300,edit_pick,s->widget_count);
  button_on(ui.body,"Готово",165,381,150,editor_exit,0);
 }else{
  for(unsigned f=0;f<16;f++){
   int y=(int)f*66;ui.edit_values[f]=bk_text(panel,"",40,y,220);
   button_on(panel,"-",0,y+12,42,edit_step,f*2);button_on(panel,"+",266,y+12,42,edit_step,f*2+1);
  }
  button_on(panel,"Удалить виджет",36,1070,240,edit_delete,0);
  ui.editor_error=bk_text(ui.body,"",65,362,350);lv_obj_set_style_text_color(ui.editor_error,lv_color_hex(0xc74732),0);
  button_on(ui.body,"Отмена",95,391,138,editor_back,0);button_on(ui.body,"Сохранить",245,391,145,edit_save,0);update_editor_labels();
 }
}
static void brightness_changed(lv_event_t *e){
 unsigned n=(unsigned)lv_slider_get_value(lv_event_get_target_obj(e));ui.brightness=n;
 char text[40];snprintf(text,sizeof(text),"Яркость %u%%",ui.brightness);label(ui.brightness_label,text);
 if(ui.brightness_callback)ui.brightness_callback(ui.brightness,ui.brightness_context);
}
static void draw_hunter(void){
 /* Own parametric side silhouette, vector geometry rather than borrowed artwork. */
 ui.boot_icon=lv_obj_create(ui.body);lv_obj_remove_style_all(ui.boot_icon);lv_obj_set_size(ui.boot_icon,280,115);lv_obj_set_pos(ui.boot_icon,100,100);
 static const lv_point_precise_t outline[]={{12,78},{12,45},{38,40},{65,8},{181,8},{217,43},{258,48},{268,78},{239,78},{233,64},{208,64},{199,78},{72,78},{64,64},{39,64},{31,78},{12,78}};
 static const lv_point_precise_t windows[]={{48,39},{74,17},{118,17},{118,40},{48,39},{128,40},{128,17},{174,17},{199,41},{128,40}};
 lv_obj_t *line=lv_line_create(ui.boot_icon);lv_line_set_points(line,outline,sizeof(outline)/sizeof(outline[0]));lv_obj_set_style_line_width(line,4,0);lv_obj_set_style_line_color(line,accent(),0);
 line=lv_line_create(ui.boot_icon);lv_line_set_points(line,windows,sizeof(windows)/sizeof(windows[0]));lv_obj_set_style_line_width(line,3,0);lv_obj_set_style_line_color(line,accent(),0);
 for(unsigned i=0;i<2;i++){
  lv_obj_t *wheel=lv_obj_create(ui.boot_icon);lv_obj_set_pos(wheel,i?204:37,65);lv_obj_set_size(wheel,30,30);lv_obj_set_style_radius(wheel,15,0);
  lv_obj_set_style_bg_color(wheel,fg(),0);lv_obj_set_style_border_width(wheel,0,0);
 }
}
static void show(void){
 if(!ui.root)return;
 if(ui.edit_mode){show_editor();return;}
 ui.rendered=0;lv_obj_clean(ui.body);ui.boot_icon=NULL;ui.brightness_label=NULL;
 int si=bk_layout_view_to_screen(ui.screen);
 uint32_t background=si>=0&&(unsigned)si<ui.layout.screen_count?ui.layout.screens[si].background:0xf7f7ef;
 lv_obj_set_style_bg_color(ui.root,lv_color_hex(ui.dark?0x101c17:background),0);lv_obj_set_style_text_color(ui.root,fg(),0);
 if(instrument(ui.screen)){
  ui.last_instrument=(unsigned)si;bk_layout_screen_t *s=&ui.layout.screens[si];
  bk_text(ui.body,s->title,85,76,310);
  for(unsigned n=0;n<s->widget_count;n++){
   bk_widget_create(&ui.widgets[n],ui.body,&s->widgets[n],ui.dark);
   if(s->widgets[n].visible){
    lv_obj_set_clickable(ui.widgets[n].root,true);
    lv_obj_add_event_cb(ui.widgets[n].root,widget_pressed,LV_EVENT_CLICKED,(void *)(uintptr_t)n);
    lv_obj_add_event_cb(ui.widgets[n].root,widget_pressed,LV_EVENT_LONG_PRESSED,(void *)(uintptr_t)n);
   }
  }ui.rendered=s->widget_count;
  button("Меню",180,381,120,5);
 }else if(ui.screen==5){
  bk_text(ui.body,"HUNTER · МЕНЮ",80,65,320);
  button("Приборы",115,113,250,bk_layout_screen_to_view(ui.last_instrument));button("Wi-Fi",115,167,250,6);
  button("Ошибки ЭБУ",115,221,250,7);button("Настройки",115,275,250,8);button("О приборе",140,329,200,9);
 }else if(ui.screen==6){
  bk_text(ui.body,"ПОДКЛЮЧЕНИЕ WI-FI",60,65,360);
  if(ui.ssid[0]){
   char data[240],ss[67],pw[131];size_t s=0,p=0;
   for(size_t i=0;ui.ssid[i];i++){if(strchr("\\;,:\"",ui.ssid[i]))ss[s++]='\\';ss[s++]=ui.ssid[i];}ss[s]=0;
   for(size_t i=0;ui.password[i];i++){if(strchr("\\;,:\"",ui.password[i]))pw[p++]='\\';pw[p++]=ui.password[i];}pw[p]=0;
   snprintf(data,sizeof(data),"WIFI:T:%s;S:%s;P:%s;;",ui.password[0]?"WPA":"nopass",ss,pw);
   lv_obj_t *qr=lv_qrcode_create(ui.body);lv_qrcode_set_size(qr,150);lv_qrcode_set_dark_color(qr,lv_color_hex(0));lv_qrcode_set_light_color(qr,lv_color_hex(0xffffff));lv_obj_set_pos(qr,165,103);lv_qrcode_update(qr,data,strlen(data));
   single_line(ui.ssid,65,263,350);char b[100];snprintf(b,sizeof(b),"Пароль: %s",ui.password);single_line(b,60,291,360);single_line(ui.url,65,322,350);
  }else bk_text(ui.body,"Точка доступа выключена\nQR появится после запуска AP",70,185,340);
  single_line(ui.wifi_status,60,351,360);button("Назад",180,391,120,5);
 }else if(ui.screen==7){
  bk_text(ui.body,"ОШИБКИ ЭБУ · ЧТЕНИЕ",70,70,340);lv_obj_t *p=scroll_panel();
  if(!ui.dtcs_valid)bk_text(p,"Список ошибок недоступен",0,20,300);
  else if(!ui.dtc_count)bk_text(p,"Ошибок не обнаружено",0,20,300);
  else {int y=0;for(size_t n=0;n<ui.dtc_count;n++){
   char b[180];const char *description=ui.dtc_lookup?ui.dtc_lookup(ui.dtcs[n]):NULL;
   snprintf(b,sizeof(b),"Код ЭБУ 0x%02X\n%s",ui.dtcs[n],description?description:"Описание не подтверждено");
   lv_obj_t *row=bk_text(p,b,0,y,300);lv_obj_update_layout(row);y+=lv_obj_get_height(row)+18;
  }}
  button("Назад",180,381,120,5);
 }else if(ui.screen==8){
  bk_text(ui.body,"НАСТРОЙКИ",100,70,280);
  button_on(ui.body,ui.dark?"Тема: тёмная":"Тема: светлая",115,113,250,theme_event,0);
  button_on(ui.body,"Редактор экранов",115,167,250,editor_list,0);
  char b[50];snprintf(b,sizeof(b),"Яркость %u%%",ui.brightness);ui.brightness_label=bk_text(ui.body,b,100,234,280);
  lv_obj_t *slider=lv_slider_create(ui.body);lv_obj_set_pos(slider,125,280);lv_obj_set_size(slider,230,20);lv_obj_set_style_bg_color(slider,accent(),LV_PART_INDICATOR);lv_obj_set_style_bg_color(slider,accent(),LV_PART_KNOB);lv_slider_set_range(slider,5,100);lv_slider_set_value(slider,ui.brightness,LV_ANIM_OFF);
  lv_obj_add_event_cb(slider,brightness_changed,LV_EVENT_VALUE_CHANGED,NULL);
  bk_text(ui.body,"Долгое нажатие на виджет:\nего параметры и расположение",70,319,340);button("Назад",180,381,120,5);
 }else if(ui.screen==9){
  draw_hunter();lv_obj_t *title=bk_text(ui.body,"HUNTER",70,225,340);lv_obj_set_style_text_font(title,&lv_font_montserrat_48,0);
  bk_text(ui.body,"БОРТОВОЙ КОМПЬЮТЕР\nМИКАС 7.2",70,297,340);button("Начать",165,381,150,bk_layout_screen_to_view(ui.last_instrument));
 }else{bk_text(ui.body,"Экран отключён",80,200,320);button("Меню",180,381,120,5);}
 refresh();
}
static void history_tick(lv_timer_t *timer){
 (void)timer;for(unsigned m=0;m<BK_METRIC_COUNT;m++)bk_widget_history_push(m,ui.values[m],ui.states[m]);
 if(!ui.edit_mode)for(unsigned n=0;n<ui.rendered;n++)bk_widget_chart_refresh(&ui.widgets[n]);
}
void bk_ui_create(lv_obj_t *parent){
 memset(&ui,0,sizeof(ui));bk_layout_defaults(&ui.layout);bk_widget_history_reset();ui.brightness=75;ui.screen=1;ui.last_instrument=1;
 ui.root=lv_obj_create(parent);lv_obj_set_size(ui.root,480,480);lv_obj_center(ui.root);lv_obj_set_scrollable(ui.root,false);
 lv_obj_set_style_pad_all(ui.root,0,0);lv_obj_set_style_border_width(ui.root,0,0);lv_obj_set_style_radius(ui.root,240,0);lv_obj_set_style_clip_corner(ui.root,true,0);
 ui.body=lv_obj_create(ui.root);lv_obj_remove_style_all(ui.body);lv_obj_set_size(ui.body,480,480);lv_obj_set_scrollable(ui.body,false);
 lv_obj_add_event_cb(ui.body,gesture,LV_EVENT_GESTURE,NULL);ui.status=bk_text(ui.root,"",100,438,280);
 ui.history_timer=lv_timer_create(history_tick,1000,NULL);show();
}
void bk_ui_set_sample(bk_metric_t m,double value,bk_value_state_t state){
 if((unsigned)m>=BK_METRIC_COUNT)return;
 bk_value_state_t next=isfinite(value)?state:BK_VALUE_UNKNOWN;
 if(ui.states[m]==next&&(ui.values[m]==value||(!isfinite(ui.values[m])&&!isfinite(value))))return;
 ui.values[m]=value;ui.states[m]=next;refresh();
}
void bk_ui_set_connected(bool connected){if(ui.connected==connected)return;ui.connected=connected;refresh();}
void bk_ui_set_theme(bool dark){ui.dark=dark;show();}
void bk_ui_select_screen(unsigned n){
 if(n>=13)return;
 if(ui.boot_timer){lv_timer_delete(ui.boot_timer);ui.boot_timer=NULL;}
 ui.edit_mode=0;ui.screen=n;show();
}
unsigned bk_ui_screen(void){return ui.screen;}
void bk_ui_set_wifi(const char *ssid,const char *password,const char *url){
 snprintf(ui.ssid,sizeof(ui.ssid),"%s",ssid?ssid:"");snprintf(ui.password,sizeof(ui.password),"%s",password?password:"");snprintf(ui.url,sizeof(ui.url),"%s",url?url:"");if(ui.screen==6)show();
}
void bk_ui_set_wifi_status(const char *status){snprintf(ui.wifi_status,sizeof(ui.wifi_status),"%s",status?status:"");if(ui.screen==6)show();}
void bk_ui_set_dtcs(const uint8_t *codes,size_t count,bool valid){
 if(count>DTC_LIMIT)count=DTC_LIMIT;
 valid=valid&&(!count||codes);
 if(ui.dtcs_valid==valid&&ui.dtc_count==count&&(!count||(codes&&!memcmp(ui.dtcs,codes,count))))return;
 ui.dtcs_valid=valid;ui.dtc_count=count;if(codes&&count)memcpy(ui.dtcs,codes,count);if(ui.screen==7)show();
}
void bk_ui_set_dtc_lookup(const char *(*lookup)(uint8_t)){ui.dtc_lookup=lookup;if(ui.screen==7)show();}
void bk_ui_set_brightness_callback(void (*cb)(uint8_t,void *),void *context){ui.brightness_callback=cb;ui.brightness_context=context;if(cb)cb(ui.brightness,context);}
void bk_ui_get_config(bk_ui_config_t *out){
 if(!out)return;
 out->screen=ui.screen;out->dark=ui.dark;out->brightness=ui.brightness;
 for(unsigned i=0;i<5;i++){
  out->selected[i]=BK_RPM;if(i>=ui.layout.screen_count)continue;
  bk_layout_screen_t *s=&ui.layout.screens[i];for(unsigned j=0;j<s->widget_count;j++)if(s->widgets[j].type==BK_WIDGET_NUMBER){out->selected[i]=s->widgets[j].metric;break;}
 }
}
bool bk_ui_set_config(const bk_ui_config_t *config){
 if(!config||config->screen>=13||config->brightness>100)return false;
 for(unsigned i=0;i<5;i++)if((unsigned)config->selected[i]>=BK_METRIC_COUNT)return false;
 ui.screen=config->screen;ui.dark=config->dark;ui.brightness=config->brightness?config->brightness:75;
 for(unsigned i=0;i<5&&i<ui.layout.screen_count;i++){
  bk_layout_screen_t *s=&ui.layout.screens[i];for(unsigned j=0;j<s->widget_count;j++)if(s->widgets[j].type==BK_WIDGET_NUMBER||s->widgets[j].type==BK_WIDGET_ARC)s->widgets[j].metric=config->selected[i];
 }
 if(ui.brightness_callback)ui.brightness_callback(ui.brightness,ui.brightness_context);
 show();return true;
}
void bk_ui_get_layout(bk_layout_t *out){if(out)*out=ui.layout;}
bool bk_ui_set_layout(const bk_layout_t *layout){
 if(!bk_layout_validate(layout))return false;
 ui.layout=*layout;
 if(ui.last_instrument>=ui.layout.screen_count||!ui.layout.screens[ui.last_instrument].enabled){
  for(unsigned n=0;n<ui.layout.screen_count;n++)if(ui.layout.screens[n].enabled){ui.last_instrument=n;break;}
 }
 if(bk_layout_view_to_screen(ui.screen)>=0&&!instrument(ui.screen)){
  for(unsigned n=0;n<ui.layout.screen_count;n++)if(ui.layout.screens[n].enabled){ui.screen=bk_layout_screen_to_view(n);break;}
 }ui.edit_mode=0;show();return true;
}
static void car_x(void *object,int32_t x){lv_obj_set_x(object,x);}
static void boot_done(lv_timer_t *timer){(void)timer;ui.boot_timer=NULL;ui.screen=ui.boot_target;show();}
void bk_ui_boot_start(unsigned next_view){
 if(next_view>=13||next_view==9)next_view=1;
 bk_ui_select_screen(9);ui.boot_target=next_view;
 lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,ui.boot_icon);lv_anim_set_values(&a,72,100);lv_anim_set_duration(&a,450);lv_anim_set_exec_cb(&a,car_x);lv_anim_start(&a);
 ui.boot_timer=lv_timer_create(boot_done,600,NULL);lv_timer_set_repeat_count(ui.boot_timer,1);
}
