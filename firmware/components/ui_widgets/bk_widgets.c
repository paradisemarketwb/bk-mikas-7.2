#include "bk_widgets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const bk_widget_registry_entry_t bk_widget_registry[BK_WIDGET_COUNT]={
 {"number","Большое число"},{"digital","Цифровой показатель"},{"arc","Круговая шкала"},
 {"needle","Стрелочный прибор"},{"bar","Индикатор уровня"},{"temperature","Температурная шкала"},
 {"status","Статус связи"},{"alarm","Предупреждение"},{"chart","Мини-график"}};
static float history[BK_METRIC_COUNT][BK_HISTORY_POINTS];
static uint8_t heads[BK_METRIC_COUNT],counts[BK_METRIC_COUNT];
static void text(lv_obj_t *o,const char *s){if(o&&strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}
lv_obj_t *bk_text(lv_obj_t *parent,const char *s,int x,int y,int width){
 lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,s);lv_obj_set_width(o,width);lv_obj_set_pos(o,x,y);
 lv_obj_set_style_text_font(o,&bk_font_ru_20,0);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);return o;
}
lv_obj_t *bk_gauge(lv_obj_t *parent,lv_color_t color){
 lv_obj_t *o=lv_arc_create(parent);lv_obj_set_size(o,414,414);lv_obj_set_pos(o,33,16);
 lv_arc_set_rotation(o,135);lv_arc_set_bg_angles(o,0,270);lv_arc_set_range(o,0,1000);lv_arc_set_value(o,0);
 lv_obj_set_style_arc_width(o,12,LV_PART_MAIN);lv_obj_set_style_arc_width(o,12,LV_PART_INDICATOR);
 lv_obj_set_style_arc_color(o,color,LV_PART_INDICATOR);lv_obj_remove_style(o,NULL,LV_PART_KNOB);lv_obj_set_clickable(o,false);return o;
}
void bk_widget_create(bk_widget_t *w,lv_obj_t *parent,const bk_widget_config_t *c,bool dark){
 memset(w,0,sizeof(*w));w->config=*c;w->dark=dark;
 lv_color_t color=lv_color_hex(dark&&c->color==0x126b48?0x72d6aa:c->color);
 if(c->type==BK_WIDGET_ARC){w->root=bk_gauge(parent,color);}
 else if(c->type==BK_WIDGET_NEEDLE){
  w->root=lv_scale_create(parent);lv_scale_set_mode(w->root,LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_range(w->root,0,1000);lv_scale_set_total_tick_count(w->root,31);
  lv_scale_set_major_tick_every(w->root,5);lv_scale_set_label_show(w->root,true);
  for(unsigned i=0;i<7;i++){snprintf(w->tick_labels[i],sizeof(w->tick_labels[i]),"%.*f",c->precision,c->min+(c->max-c->min)*i/6.0);w->tick_ptrs[i]=w->tick_labels[i];}
  w->tick_ptrs[7]=NULL;lv_scale_set_text_src(w->root,w->tick_ptrs);
  lv_obj_set_style_text_font(w->root,&bk_font_ru_20,LV_PART_INDICATOR);lv_obj_set_style_text_color(w->root,lv_color_hex(dark?0xe9f1ed:0x112b20),LV_PART_INDICATOR);
  lv_scale_set_angle_range(w->root,270);lv_scale_set_rotation(w->root,135);
  lv_obj_set_style_line_color(w->root,color,LV_PART_MAIN);lv_obj_set_style_line_color(w->root,color,LV_PART_ITEMS);lv_obj_set_style_line_color(w->root,color,LV_PART_INDICATOR);
  lv_obj_set_style_line_width(w->root,2,LV_PART_MAIN);lv_obj_set_style_line_width(w->root,2,LV_PART_ITEMS);lv_obj_set_style_line_width(w->root,3,LV_PART_INDICATOR);
  lv_obj_set_style_length(w->root,6,LV_PART_ITEMS);lv_obj_set_style_length(w->root,12,LV_PART_INDICATOR);
  w->needle=lv_line_create(w->root);lv_obj_set_style_line_width(w->needle,4,0);lv_obj_set_style_line_color(w->needle,color,0);
 }else{
  w->root=lv_obj_create(parent);lv_obj_remove_style_all(w->root);lv_obj_set_scrollable(w->root,false);
  if(c->type==BK_WIDGET_CHART){
   w->value=bk_text(w->root,"",0,0,c->w);
   w->chart=lv_chart_create(w->root);lv_obj_set_pos(w->chart,5,28);lv_obj_set_size(w->chart,c->w-10,c->h-32);
   lv_obj_set_style_bg_color(w->chart,lv_color_hex(dark?0x172b20:0xffffff),0);
   lv_obj_set_style_line_color(w->chart,lv_color_hex(dark?0x385242:0xdce5dd),LV_PART_MAIN);
   lv_chart_set_type(w->chart,LV_CHART_TYPE_LINE);lv_chart_set_point_count(w->chart,BK_HISTORY_POINTS);
   lv_chart_set_axis_range(w->chart,LV_CHART_AXIS_PRIMARY_Y,0,1000);lv_chart_set_div_line_count(w->chart,3,3);
   lv_obj_set_style_size(w->chart,0,0,LV_PART_INDICATOR);
   w->series=lv_chart_add_series(w->chart,color,LV_CHART_AXIS_PRIMARY_Y);
  }else if(c->type==BK_WIDGET_BAR||c->type==BK_WIDGET_TEMPERATURE){
   w->value=bk_text(w->root,"",0,0,c->w);
   w->chart=lv_bar_create(w->root);lv_obj_set_pos(w->chart,4,c->h-20);lv_obj_set_size(w->chart,c->w-8,16);
   lv_bar_set_range(w->chart,0,1000);lv_obj_set_style_bg_color(w->chart,color,LV_PART_INDICATOR);
  }else if(c->type==BK_WIDGET_NUMBER){
   w->value=bk_text(w->root,"",0,30,c->w);w->unit=bk_text(w->root,"",0,c->h-27,c->w);
   w->chart=bk_text(w->root,"",0,0,c->w); /* title label, not a chart object */
  }else w->value=bk_text(w->root,"",0,0,c->w);
 }
 lv_obj_set_pos(w->root,c->x,c->y);lv_obj_set_size(w->root,c->w,c->h);
 if(c->type==BK_WIDGET_NEEDLE){int side=c->w<c->h?c->w:c->h;lv_obj_set_size(w->root,side,side);lv_obj_set_pos(w->root,c->x+(c->w-side)/2,c->y+(c->h-side)/2);}
 lv_obj_set_hidden(w->root,!c->visible);
}
void bk_widget_update(bk_widget_t *w,double value,bk_value_state_t state,bool connected,const char *name,const char *unit){
 const bk_widget_config_t *c=&w->config;
 bool valid=state==BK_VALUE_VALID&&isfinite(value),warning=valid&&(((c->flags&BK_WARN_LOW)&&value<c->warn_low)||((c->flags&BK_WARN_HIGH)&&value>=c->warn_high));
 lv_color_t color=lv_color_hex(warning?0xc74732:w->dark&&c->color==0x126b48?0x72d6aa:c->color);
 lv_color_t fg=lv_color_hex(w->dark?0xe9f1ed:0x112b20);
 double ratio=valid?(value-c->min)/(c->max-c->min):0;if(ratio<0)ratio=0;if(ratio>1)ratio=1;int n=(int)(ratio*1000);
 const char *u=c->unit[0]?c->unit:unit,*state_text="--";
 if(state==BK_VALUE_STALE)state_text="Устарело";else if(state==BK_VALUE_UNSUPPORTED)state_text="Недоступно";else if(state==BK_VALUE_INSUFFICIENT)state_text="Нет данных";
 char number[64],buffer[192];if(valid)snprintf(number,sizeof(number),"%.*f",c->precision,value);else snprintf(number,sizeof(number),"%s",state_text);
 switch(c->type){
 case BK_WIDGET_ARC:lv_arc_set_value(w->root,n);lv_obj_set_style_arc_color(w->root,color,LV_PART_INDICATOR);break;
 case BK_WIDGET_NEEDLE:lv_scale_set_line_needle_value(w->root,w->needle,(c->w<c->h?c->w:c->h)/2-14,n);lv_obj_set_style_line_color(w->needle,color,0);break;
 case BK_WIDGET_NUMBER:
  text(w->value,number);text(w->unit,u);text(w->chart,name);
  lv_obj_set_style_text_font(w->value,valid&&c->font_size==48?&lv_font_montserrat_48:&bk_font_ru_20,0);
  lv_obj_set_style_text_color(w->value,warning?color:fg,0);break;
 case BK_WIDGET_STATUS:lv_obj_set_style_text_color(w->value,fg,0);text(w->value,connected?"ЭБУ: подключён":"ЭБУ: нет связи");break;
 case BK_WIDGET_ALARM:
  snprintf(buffer,sizeof(buffer),warning?"ВНИМАНИЕ\n%s: %s %s":"%s\n%s %s",name,number,u);text(w->value,buffer);
  lv_obj_set_style_text_color(w->value,warning?color:fg,0);break;
 case BK_WIDGET_CHART:snprintf(buffer,sizeof(buffer),"%s · 64 с",name);text(w->value,buffer);lv_obj_set_style_text_color(w->value,fg,0);break;
 default:
  snprintf(buffer,sizeof(buffer),"%s\n%s %s",name,number,u);text(w->value,buffer);lv_obj_set_style_text_color(w->value,warning?color:fg,0);
  if(c->type==BK_WIDGET_BAR||c->type==BK_WIDGET_TEMPERATURE){lv_bar_set_value(w->chart,n,LV_ANIM_OFF);lv_obj_set_style_bg_color(w->chart,color,LV_PART_INDICATOR);}
  break;
 }
}
void bk_widget_history_reset(void){memset(heads,0,sizeof(heads));memset(counts,0,sizeof(counts));}
void bk_widget_history_push(bk_metric_t metric,double value,bk_value_state_t state){
 if((unsigned)metric>=BK_METRIC_COUNT)return;
 history[metric][heads[metric]]=state==BK_VALUE_VALID&&isfinite(value)?(float)value:NAN;
 heads[metric]=(heads[metric]+1)%BK_HISTORY_POINTS;if(counts[metric]<BK_HISTORY_POINTS)counts[metric]++;
}
size_t bk_widget_history_count(bk_metric_t metric){return (unsigned)metric<BK_METRIC_COUNT?counts[metric]:0;}
void bk_widget_chart_refresh(bk_widget_t *w){
 if(w->config.type!=BK_WIDGET_CHART)return;
 unsigned m=w->config.metric;
 int32_t data[BK_HISTORY_POINTS];unsigned count=counts[m],start=(heads[m]+BK_HISTORY_POINTS-count)%BK_HISTORY_POINTS;
 for(unsigned i=0;i<BK_HISTORY_POINTS;i++){
  data[i]=LV_CHART_POINT_NONE;
  if(i>=BK_HISTORY_POINTS-count){float v=history[m][(start+i-(BK_HISTORY_POINTS-count))%BK_HISTORY_POINTS];
   if(isfinite(v)){double ratio=(v-w->config.min)/(w->config.max-w->config.min);if(ratio<0)ratio=0;if(ratio>1)ratio=1;data[i]=(int32_t)(ratio*1000);}
  }
 }
 lv_chart_set_series_values(w->chart,w->series,data,BK_HISTORY_POINTS);lv_chart_refresh(w->chart);
}
