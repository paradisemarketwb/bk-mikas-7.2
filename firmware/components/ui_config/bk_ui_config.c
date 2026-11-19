#include "bk_ui_config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_layout_view_to_screen(unsigned v){return v<5?(int)v:v>=10&&v<13?(int)v-5:-1;}
unsigned bk_layout_screen_to_view(unsigned s){return s<5?s:s+5;}
static bool terminated(const char *s,size_t n){return memchr(s,0,n)!=NULL;}
static bool inside(int x,int y){int dx=x-240,dy=y-240;return dx*dx+dy*dy<=232*232;}
bool bk_layout_validate(const bk_layout_t *l){
 if(!l||l->version!=BK_LAYOUT_VERSION||l->screen_count<1||l->screen_count>BK_LAYOUT_MAX_SCREENS)return false;
 unsigned enabled=0;
 for(unsigned i=0;i<l->screen_count;i++){
  const bk_layout_screen_t *s=&l->screens[i];enabled+=s->enabled;
  if(!terminated(s->id,sizeof(s->id))||!s->id[0]||!terminated(s->title,sizeof(s->title))||s->background>0xffffff||s->widget_count>BK_LAYOUT_MAX_WIDGETS)return false;
  for(unsigned n=0;s->id[n];n++)if(!((s->id[n]>='a'&&s->id[n]<='z')||(s->id[n]>='A'&&s->id[n]<='Z')||(s->id[n]>='0'&&s->id[n]<='9')||s->id[n]=='_'||s->id[n]=='-'))return false;
  for(unsigned j=0;j<i;j++)if(!strcmp(s->id,l->screens[j].id))return false;
  for(unsigned j=0;j<s->widget_count;j++){
   const bk_widget_config_t *w=&s->widgets[j];
   if((unsigned)w->type>=BK_WIDGET_COUNT||(unsigned)w->metric>=BK_METRIC_COUNT||w->x<0||w->y<0||w->w<16||w->h<16||w->x+w->w>480||w->y+w->h>432)return false;
   if((w->type==BK_WIDGET_NUMBER&&(w->w<120||w->h<110))||
      ((w->type==BK_WIDGET_CHART||w->type==BK_WIDGET_BAR||w->type==BK_WIDGET_TEMPERATURE)&&(w->w<64||w->h<64))||
      ((w->type==BK_WIDGET_DIGITAL||w->type==BK_WIDGET_ALARM)&&w->h<48))return false;
   if((w->flags&(BK_WARN_LOW|BK_WARN_HIGH))==(BK_WARN_LOW|BK_WARN_HIGH)&&w->warn_low>=w->warn_high)return false;
   if(w->color>0xffffff||w->flags>3||w->precision>3||(w->font_size!=20&&w->font_size!=48)||!terminated(w->unit,sizeof(w->unit)))return false;
   if(!isfinite(w->min)||!isfinite(w->max)||!isfinite(w->warn_low)||!isfinite(w->warn_high)||w->min>=w->max||fabsf(w->min)>1000000||fabsf(w->max)>1000000)return false;
   if(w->type==BK_WIDGET_ARC||w->type==BK_WIDGET_NEEDLE){
    /* The circular scale is inscribed in its box, so rectangular corners are irrelevant. */
    int cx=w->x+w->w/2,cy=w->y+w->h/2,r=(w->w<w->h?w->w:w->h)/2;
    double dx=cx-240,dy=cy-240;if(sqrt(dx*dx+dy*dy)+r>232)return false;
   }else if(!inside(w->x,w->y)||!inside(w->x+w->w,w->y)||!inside(w->x,w->y+w->h)||!inside(w->x+w->w,w->y+w->h))return false;
  }
 }
 return enabled>0;
}
static bk_widget_config_t widget(bk_widget_type_t t,bk_metric_t m,int x,int y,int w,int h,float maximum){
 bk_widget_config_t a={.type=t,.metric=m,.x=x,.y=y,.w=w,.h=h,.min=0,.max=maximum,.color=0x126b48,.font_size=20,.visible=true};
 if(t==BK_WIDGET_NUMBER)a.font_size=48;
 if(m==BK_VOLTAGE||m==BK_FUEL_LPH||m==BK_INJECTION)a.precision=1;
 if(m==BK_COOLANT){a.flags=BK_WARN_HIGH;a.warn_high=105;}
 if(m==BK_RPM){a.flags=BK_WARN_HIGH;a.warn_high=5800;}
 if(m==BK_VOLTAGE){a.flags=BK_WARN_LOW;a.warn_low=12;}
 return a;
}
void bk_layout_defaults(bk_layout_t *l){
 if(!l)return;
 memset(l,0,sizeof(*l));l->version=BK_LAYOUT_VERSION;l->screen_count=5;
 const char *ids[]={"main","engine","fuel","sensors","trip"};
 const char *titles[]={"ГЛАВНЫЙ ПРИБОР","ДВИГАТЕЛЬ","ТОПЛИВО","ДАТЧИКИ","ПОЕЗДКА"};
 const bk_metric_t main[]={BK_SPEED,BK_COOLANT,BK_FUEL_LPH,BK_MAF,BK_DISTANCE};
 const bk_metric_t left[]={BK_RPM,BK_VOLTAGE,BK_RANGE,BK_LAMBDA,BK_TRIP_SECONDS};
 const bk_metric_t right[]={BK_VOLTAGE,BK_RPM,BK_INJECTION,BK_IGNITION,BK_AVG_SPEED};
 const float maxima[]={160,120,30,200,1000};
 for(unsigned i=0;i<5;i++){
  bk_layout_screen_t *s=&l->screens[i];snprintf(s->id,sizeof(s->id),"%s",ids[i]);snprintf(s->title,sizeof(s->title),"%s",titles[i]);s->enabled=true;s->background=0xf7f7ef;s->widget_count=4;
  s->widgets[0]=widget(BK_WIDGET_ARC,main[i],33,16,414,414,maxima[i]);
  s->widgets[1]=widget(BK_WIDGET_NUMBER,main[i],85,150,310,130,maxima[i]);
  s->widgets[2]=widget(BK_WIDGET_DIGITAL,left[i],70,295,165,75,6000);
  s->widgets[3]=widget(BK_WIDGET_DIGITAL,right[i],245,295,165,75,6000);
 }
}
