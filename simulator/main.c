#include "bk_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef BK_HAVE_SDL
#include <SDL.h>
#endif
static uint16_t framebuffer[480*480], draw_buffer[480*40];
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
 bk_ui_set_sample(BK_DTC_COUNT,0,state);bk_ui_set_sample(BK_LAMBDA,128,state);
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
    case SDLK_d:bk_ui_set_sample(BK_DTC_COUNT,3,BK_VALUE_VALID);bk_ui_select_screen(7);break;
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
int main(int argc,char **argv){
 lv_init();lv_display_t *d=lv_display_create(480,480);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 bk_ui_create(lv_screen_active());sample();
 if(argc>1&&!strcmp(argv[1],"--headless")){
  const char *dir=argc>2?argv[2]:".";char path[1024];
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
  lv_indev_delete(test_pointer);
  bk_ui_config_t saved,changed,readback;
  bk_ui_get_config(&saved);changed=saved;changed.selected[0]=BK_THROTTLE;changed.screen=0;
  if(!bk_ui_set_config(&changed)){fprintf(stderr,"Valid UI config rejected\n");return 1;}
  bk_ui_get_config(&readback);
  if(readback.screen!=0||readback.selected[0]!=BK_THROTTLE){fprintf(stderr,"UI config roundtrip failed\n");return 1;}
  changed.selected[2]=BK_METRIC_COUNT;
  if(bk_ui_set_config(&changed)){fprintf(stderr,"Invalid metric accepted\n");return 1;}
  if(!bk_ui_set_config(&saved))return 1;
  printf("Rendered 10 real LVGL screens in both themes (RGB565 480x480); touch menu/settings passed.\n");
  lv_display_delete(d);lv_deinit();return 0;
 }
#ifdef BK_HAVE_SDL
 int result=interactive();lv_display_delete(d);lv_deinit();return result;
#else
 fprintf(stderr,"SDL2 unavailable. Use --headless OUTPUT_DIRECTORY or rebuild with SDL2.\n");return 2;
#endif
}
