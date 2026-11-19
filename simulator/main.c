#include "bk_ui.h"
#include "bk_widgets.h"
#include "bk_settings.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef BK_HAVE_SDL
#include <SDL.h>
#endif
static uint16_t framebuffer[480*480], draw_buffer[480*40];
static uint8_t dtc_codes[]={0x12,0x34,0x56};static size_t dtc_count;
static bool connected=true,paused=false;static double rpm=2400,temp=88,voltage=13.8,throttle=18,fuel=4.1;
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){
 const uint16_t *src=(const uint16_t *)pixels;
 for(int y=area->y1;y<=area->y2;y++){
  memcpy(framebuffer+y*480+area->x1,src,(size_t)(area->x2-area->x1+1)*2);
  src+=area->x2-area->x1+1;
 }
 lv_display_flush_ready(display);
}
static void sample(void){
 bk_ui_set_connected(connected);
 bk_value_state_t state=connected?BK_VALUE_VALID:BK_VALUE_STALE;
 bk_ui_set_sample(BK_RPM,rpm,state);bk_ui_set_sample(BK_COOLANT,temp,state);
 bk_ui_set_sample(BK_VOLTAGE,voltage,state);bk_ui_set_sample(BK_THROTTLE,throttle,state);
 bk_ui_set_sample(BK_FUEL_LPH,fuel,state);bk_ui_set_sample(BK_MAF,14.5,state);
 bk_ui_set_sample(BK_IGNITION,16,state);bk_ui_set_sample(BK_INJECTION,4.2,state);
 bk_ui_set_sample(BK_DTC_COUNT,(double)dtc_count,state);bk_ui_set_dtcs(dtc_codes,dtc_count,connected);bk_ui_set_sample(BK_LAMBDA,128,state);
 /* No fabricated speed, range, distance or trip averages. */
}
static int ppm(const char *path){
 FILE *f=fopen(path,"wb");if(!f){perror(path);return 1;}
 fprintf(f,"P6\n480 480\n255\n");
 for(unsigned i=0;i<480*480;i++){
  uint16_t c=framebuffer[i];uint8_t rgb[3]={(uint8_t)(((c>>11)&31)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};
  if(fwrite(rgb,1,3,f)!=3){fclose(f);return 1;}
 }
 return fclose(f)!=0;
}
static int mouse_x,mouse_y;static bool mouse_down;
static void pointer_read(lv_indev_t *indev,lv_indev_data_t *data){(void)indev;data->point.x=mouse_x;data->point.y=mouse_y;data->state=mouse_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t *pointer,int x,int y){
 mouse_x=x;mouse_y=y;mouse_down=false;
 for(unsigned i=0;i<3;i++){lv_tick_inc(20);lv_indev_read(pointer);lv_timer_handler();}
 mouse_down=true;
 for(unsigned i=0;i<4;i++){lv_tick_inc(20);lv_indev_read(pointer);lv_timer_handler();}
 mouse_down=false;
 for(unsigned i=0;i<4;i++){lv_tick_inc(20);lv_indev_read(pointer);lv_timer_handler();}
}
#ifdef BK_HAVE_SDL
static int interactive(void){
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)){fprintf(stderr,"%s\n",SDL_GetError());return 1;}
 SDL_Window *window=SDL_CreateWindow("HUNTER - SIMULATION | arrows: page, T: theme, C: ECU, +/-: RPM",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,480,480,0);
 SDL_Renderer *renderer=window?SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE):NULL;
 SDL_Texture *texture=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,480,480):NULL;
 if(!texture){fprintf(stderr,"%s\n",SDL_GetError());SDL_Quit();return 1;}
 lv_indev_t *pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,pointer_read);
 bool running=true,dark=false;uint32_t previous=SDL_GetTicks(),last_sample=previous;
 while(running){
  SDL_Event e;while(SDL_PollEvent(&e)){
   if(e.type==SDL_QUIT)running=false;
   if(e.type==SDL_MOUSEMOTION){mouse_x=e.motion.x;mouse_y=e.motion.y;}
   if(e.type==SDL_MOUSEBUTTONDOWN)mouse_down=true;
   if(e.type==SDL_MOUSEBUTTONUP)mouse_down=false;
   if(e.type==SDL_KEYDOWN){switch(e.key.keysym.sym){
    case SDLK_ESCAPE:running=false;break;
    case SDLK_RIGHT:bk_ui_select_screen((bk_ui_screen()+1)%10);break;
    case SDLK_LEFT:bk_ui_select_screen((bk_ui_screen()+9)%10);break;
    case SDLK_t:dark=!dark;bk_ui_set_theme(dark);break;
    case SDLK_c:connected=!connected;sample();break;
    case SDLK_SPACE:paused=!paused;break;
    case SDLK_EQUALS:case SDLK_PLUS:rpm+=200;sample();break;
    case SDLK_MINUS:rpm=rpm>200?rpm-200:0;sample();break;
    case SDLK_w:temp+=2;sample();break;case SDLK_s:temp-=2;sample();break;
    case SDLK_v:voltage=voltage>12?11.5:13.8;sample();break;
    case SDLK_d:dtc_count=dtc_count?0:3;sample();bk_ui_select_screen(7);break;
    default:break;}
   }
  }
  uint32_t now=SDL_GetTicks();lv_tick_inc(now-previous);previous=now;
  if(!paused&&now-last_sample>500){last_sample=now;sample();}
  lv_timer_handler();SDL_UpdateTexture(texture,NULL,framebuffer,480*2);
  SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,NULL,NULL);SDL_RenderPresent(renderer);SDL_Delay(10);
 }
 lv_indev_delete(pointer);SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
#endif
static bool import_profile(const char *path){
 FILE *f=fopen(path,"rb");if(!f){perror(path);return false;}
 char *json=malloc(32770);if(!json){fclose(f);return false;}
 size_t n=fread(json,1,32769,f);bool io=ferror(f)!=0;fclose(f);
 if(io||n>32768){free(json);fprintf(stderr,"Profile too large or unreadable\n");return false;}json[n]=0;
 bk_settings_t s;bool valid=bk_settings_from_json(json,&s);free(json);
 if(!valid){fprintf(stderr,"Invalid profile: rejected without changing UI\n");return false;}
 bk_ui_config_t c={.screen=s.screen,.dark=s.dark,.brightness=s.brightness};
 for(unsigned i=0;i<5;i++)c.selected[i]=s.selected[i];
 return bk_ui_set_config(&c)&&bk_ui_set_layout(&s.layout);
}
static bool export_profile(const char *path){
 bk_settings_t s;bk_settings_defaults(&s);bk_ui_config_t c;bk_ui_get_config(&c);
 s.screen=c.screen;s.dark=c.dark;s.brightness=c.brightness;
 for(unsigned i=0;i<5;i++)s.selected[i]=c.selected[i];
 bk_ui_get_layout(&s.layout);
 char *json=bk_settings_to_json(&s);if(!json)return false;
 FILE *f=fopen(path,"wb");if(!f){perror(path);cJSON_free(json);return false;}
 size_t n=strlen(json);bool ok=fwrite(json,1,n,f)==n;if(fclose(f))ok=false;cJSON_free(json);return ok;
}
int main(int argc,char **argv){
 const char *input=NULL,*output=NULL,*render=NULL,*headless=NULL;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--import")&&i+1<argc)input=argv[++i];
  else if(!strcmp(argv[i],"--export")&&i+1<argc)output=argv[++i];
  else if(!strcmp(argv[i],"--render")&&i+1<argc)render=argv[++i];
  else if(!strcmp(argv[i],"--headless")){headless=i+1<argc?argv[++i]:".";}
  else{fprintf(stderr,"Usage: bk_sim [--import profile.json] [--export profile.json] [--render DIR | --headless DIR]\n");return 2;}
 }

 lv_init();lv_display_t *d=lv_display_create(480,480);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 bk_ui_create(lv_screen_active());sample();
 if(input&&!import_profile(input))return 1;
 if(render){
  char path[1024];unsigned saved=bk_ui_screen();
  for(unsigned view=0;view<13;view++){
   bk_ui_select_screen(view);lv_tick_inc(40);lv_timer_handler();lv_refr_now(d);
   snprintf(path,sizeof(path),"%s/custom-%02u.ppm",render,view);if(ppm(path))return 1;
  }
  bk_ui_select_screen(saved);if(output&&!export_profile(output))return 1;lv_display_delete(d);lv_deinit();return 0;
 }
 if(headless){
  const char *dir=headless;char path[1024];
  if(output&&!export_profile(output))return 1;
  /* The CTest suite always starts from defaults, independently of imported profile. */
  bk_layout_t test_defaults;bk_layout_defaults(&test_defaults);if(!bk_ui_set_layout(&test_defaults))return 1;
  /* Clearly marked simulator-only credentials, never a production default. */
  bk_ui_set_wifi("HUNTER-SIMULATION","SIMULATOR-ONLY","http://192.168.4.1");
  for(unsigned theme=0;theme<2;theme++){
   bk_ui_set_theme(theme!=0);
   for(unsigned screen=0;screen<10;screen++){
    bk_ui_select_screen(screen);lv_tick_inc(40);lv_timer_handler();lv_refr_now(d);
    snprintf(path,sizeof(path),"%s/screen-%02u-%s.ppm",dir,screen,theme?"dark":"light");if(ppm(path))return 1;
   }
  }
  bk_ui_set_sample(BK_RPM,0,BK_VALUE_STALE);bk_ui_set_connected(false);
  bk_ui_select_screen(1);lv_refr_now(d);
  lv_indev_t *test_pointer=lv_indev_create();
  lv_indev_set_type(test_pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(test_pointer,pointer_read);
  mouse_x=240;mouse_y=402;mouse_down=true;lv_indev_read(test_pointer);
  lv_tick_inc(30);mouse_down=false;lv_indev_read(test_pointer);
  if(bk_ui_screen()!=5){fprintf(stderr,"Touch menu navigation failed\n");return 1;}
  lv_refr_now(d);
  mouse_x=240;mouse_y=298;mouse_down=true;lv_indev_read(test_pointer);
  lv_tick_inc(30);mouse_down=false;lv_indev_read(test_pointer);
  if(bk_ui_screen()!=8){fprintf(stderr,"Touch settings navigation failed\n");return 1;}
  bk_layout_t before_editor,after_editor;bk_ui_get_layout(&before_editor);
  tap(test_pointer,240,190);lv_refr_now(d);
  snprintf(path,sizeof(path),"%s/layout-editor.ppm",dir);if(ppm(path))return 1;
  tap(test_pointer,240,195);lv_refr_now(d);
  snprintf(path,sizeof(path),"%s/widget-editor.ppm",dir);if(ppm(path))return 1;
  tap(test_pointer,370,150);
  tap(test_pointer,310,410);
  bk_ui_get_layout(&after_editor);
  if(after_editor.screens[1].widgets[0].type!=BK_WIDGET_NEEDLE){fprintf(stderr,"Live editor widget-type apply failed\n");return 1;}
  if(!bk_ui_set_layout(&before_editor))return 1;
  lv_indev_delete(test_pointer);
  bk_ui_config_t saved,changed,readback;
  bk_ui_get_config(&saved);changed=saved;changed.selected[0]=BK_THROTTLE;changed.screen=0;
  if(!bk_ui_set_config(&changed)){fprintf(stderr,"Valid UI config rejected\n");return 1;}
  bk_ui_get_config(&readback);
  if(readback.screen!=0||readback.selected[0]!=BK_THROTTLE){fprintf(stderr,"UI config roundtrip failed\n");return 1;}
  changed.selected[2]=BK_METRIC_COUNT;
  if(bk_ui_set_config(&changed)){fprintf(stderr,"Invalid metric accepted\n");return 1;}
  if(!bk_ui_set_config(&saved))return 1;
  bk_layout_t original,custom;bk_ui_get_layout(&original);
  if(!bk_layout_validate(&original)){fprintf(stderr,"Invalid default layout\n");return 1;}
  custom=original;custom.screen_count=1;custom.screens[0].widget_count=1;
  bk_widget_config_t *w=&custom.screens[0].widgets[0];
  *w=(bk_widget_config_t){.metric=BK_RPM,.x=100,.y=120,.w=280,.h=230,.min=0,.max=6000,.color=0x126b48,.visible=true,.font_size=48};
  bk_ui_set_connected(true);bk_ui_set_sample(BK_RPM,2400,BK_VALUE_VALID);
  for(unsigned kind=0;kind<BK_WIDGET_COUNT;kind++){
   w->type=kind;if(!bk_ui_set_layout(&custom)){fprintf(stderr,"Valid widget layout rejected %u\n",kind);return 1;}
   bk_ui_select_screen(0);
   for(unsigned k=0;k<70;k++){bk_ui_set_sample(BK_RPM,1000+(k%10)*350,BK_VALUE_VALID);lv_tick_inc(1000);lv_timer_handler();}
   lv_refr_now(d);snprintf(path,sizeof(path),"%s/widget-%02u.ppm",dir,kind);if(ppm(path))return 1;
  }
  if(bk_widget_history_count(BK_RPM)!=BK_HISTORY_POINTS){fprintf(stderr,"History not bounded at64\n");return 1;}
  custom.screens[0].widgets[0].type=BK_WIDGET_NUMBER;
  custom.screens[0].widgets[0].x=0;custom.screens[0].widgets[0].y=0;
  if(bk_ui_set_layout(&custom)){fprintf(stderr,"Off-circle layout accepted\n");return 1;}
  if(!bk_ui_set_layout(&original))return 1;
  bk_ui_boot_start(1);lv_tick_inc(650);lv_timer_handler();
  if(bk_ui_screen()!=1){fprintf(stderr,"Nonblocking boot did not finish\n");return 1;}
  printf("LVGL10screens/2themes, pointer navigation, config/layout validation,9widgettypes,64-point history,600msboot: passed.\n");
  lv_display_delete(d);lv_deinit();return 0;
 }
#ifdef BK_HAVE_SDL
 int result=interactive();if(output&&!export_profile(output))result=1;lv_display_delete(d);lv_deinit();return result;
#else
 fprintf(stderr,"SDL2 unavailable. Use --headless OUTPUT_DIRECTORY or rebuild with SDL2.\n");return 2;
#endif
}
