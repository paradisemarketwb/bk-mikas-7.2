#include "bk_telemetry.h"
#include "bk_kline.h"
#include "bk_settings.h"
#include "bk_wifi.h"
#include "bk_ui.h"
#include "bk_board.h"
#include "mikas.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>
static bk_snapshot_t store;
static SemaphoreHandle_t store_lock;
static QueueHandle_t settings_queue;
static void settings_task(void *arg){
 (void)arg;bk_settings_t v;for(;;)if(xQueueReceive(settings_queue,&v,portMAX_DELAY)==pdTRUE){
  esp_err_t e=bk_settings_save(&v);if(e)ESP_LOGW("settings","Save failed: %s",esp_err_to_name(e));
 }
}
static const char *TAG="bk";
static uint64_t now_ms(void){return esp_timer_get_time()/1000;}
static void snapshot(bk_snapshot_t *s){xSemaphoreTake(store_lock,portMAX_DELAY);*s=store;xSemaphoreGive(store_lock);}
#if CONFIG_BK_ENABLE_KLINE
static void ecu_task(void *arg){
 (void)arg;uint8_t reply[256];size_t n;
 if(bk_kline_init()!=ESP_OK){bk_kline_disarm();ESP_LOGE(TAG,"K-Line init failed");vTaskDelete(NULL);return;}
 for(;;){
  uint8_t version=1;
  if(bk_kline_exchange(&version,1,reply,sizeof(reply),&n,600)!=ESP_OK||n!=1||reply[0]!=0x0b){vTaskDelay(pdMS_TO_TICKS(1250));continue;}
  xSemaphoreTake(store_lock,portMAX_DELAY);bk_store_init(&store);store.connected=true;xSemaphoreGive(store_lock);
  uint64_t due[TEL_METRIC_COUNT]={0},last_good=now_ms(),fault_due=now_ms()+10000;
  unsigned cursor=0;
  while(now_ms()-last_good<3000){
   uint64_t now=now_ms();int selected=-1;
   /* Round-robin among overdue fields: bounded queue of one, no concurrent UART users. */
   for(unsigned i=0;i<TEL_METRIC_COUNT;i++){unsigned m=(cursor+i)%TEL_METRIC_COUNT;if(now>=due[m]){selected=m;cursor=(m+1)%TEL_METRIC_COUNT;break;}}
   if(selected>=0){
    const bk_field_t *f=&bk_fields[selected];uint8_t cmd[]={0x61,f->pid};due[selected]=now+f->period_ms;
    if(bk_kline_exchange(cmd,2,reply,sizeof(reply),&n,600)==ESP_OK&&n==f->width){
     last_good=now_ms();xSemaphoreTake(store_lock,portMAX_DELAY);bk_store_reply(&store,selected,reply,n,last_good);xSemaphoreGive(store_lock);
    }
   }else vTaskDelay(pdMS_TO_TICKS(10));
   if(now_ms()>=fault_due){
    uint8_t cmd=2;fault_due=now_ms()+10000;
    bool ok=bk_kline_exchange(&cmd,1,reply,sizeof(reply),&n,600)==ESP_OK&&mikas_faults_valid(reply,n);
    xSemaphoreTake(store_lock,portMAX_DELAY);store.faults_valid=ok;
    if(ok){store.fault_count=reply[0];memcpy(store.faults,reply+1,n-1);store.faults_at_ms=now_ms();}xSemaphoreGive(store_lock);
   }
  }
  xSemaphoreTake(store_lock,portMAX_DELAY);store.connected=false;store.faults_valid=false;xSemaphoreGive(store_lock);
 }
}
#endif
static void ui_task(void *arg){
 bk_settings_t cfg=*(bk_settings_t *)arg;
 bk_ui_create(lv_screen_active());
 bk_ui_config_t initial={.screen=cfg.screen,.dark=cfg.dark};
 for(unsigned i=0;i<5;i++)initial.selected[i]=cfg.selected[i];
 bk_ui_set_config(&initial);
 uint64_t changed_at=0;bool dirty=false;
#if CONFIG_BK_ENABLE_WIFI
 bk_ui_set_wifi(bk_wifi_ssid(),bk_wifi_password(),"http://192.168.4.1/");
#endif
 static const bk_metric_t map[TEL_METRIC_COUNT]={BK_RPM,BK_COOLANT,BK_VOLTAGE,BK_THROTTLE,BK_METRIC_COUNT,BK_IGNITION,BK_MAF,BK_FUEL_LPH,BK_INJECTION,BK_LAMBDA,BK_METRIC_COUNT};
 uint64_t last=now_ms();
 for(;;){
  bk_snapshot_t s;snapshot(&s);uint64_t now=now_ms();lv_tick_inc(now-last);last=now;
  bk_ui_set_connected(s.connected);
  for(unsigned i=0;i<TEL_METRIC_COUNT;i++)if(map[i]!=BK_METRIC_COUNT){
   bk_sample_t v=bk_store_get(&s,i,now);bk_value_state_t state=BK_VALUE_UNKNOWN;
   if(v.state==TEL_VALID)state=BK_VALUE_VALID;else if(v.state==TEL_STALE)state=BK_VALUE_STALE;else if(v.state==TEL_NEEDS_DATA)state=BK_VALUE_INSUFFICIENT;
   bk_ui_set_sample(map[i],v.value,state);
  }
  bk_ui_set_sample(BK_DTC_COUNT,s.fault_count,s.connected&&s.faults_valid?BK_VALUE_VALID:BK_VALUE_UNKNOWN);
  lv_timer_handler();
  bk_ui_config_t current;bk_ui_get_config(&current);
  bool changed=cfg.screen!=current.screen||cfg.dark!=current.dark;
  for(unsigned i=0;i<5;i++)if(cfg.selected[i]!=current.selected[i])changed=true;
  if(changed){cfg.screen=current.screen;cfg.dark=current.dark;for(unsigned i=0;i<5;i++)cfg.selected[i]=current.selected[i];changed_at=now;dirty=true;}
  if(dirty&&now-changed_at>=2000&&settings_queue){xQueueOverwrite(settings_queue,&cfg);dirty=false;}
  vTaskDelay(pdMS_TO_TICKS(20));
 }
}
void app_main(void){
 esp_err_t e=nvs_flash_init();
 if(e!=ESP_OK){ESP_LOGE(TAG,"NVS init failed (%s); preserve settings, recover manually",esp_err_to_name(e));return;}
 store_lock=xSemaphoreCreateMutex();if(!store_lock){ESP_LOGE(TAG,"No memory for telemetry mutex");return;}
 bk_store_init(&store);static bk_settings_t settings;
 e=bk_settings_load(&settings);if(e)ESP_LOGW(TAG,"Settings invalid; defaults selected");
/* Reserve display DMA/bounce buffers before Wi-Fi allocates its buffers. */
 esp_err_t display_status=bk_board_start();
#if CONFIG_BK_ENABLE_WIFI
 e=bk_wifi_start(snapshot);if(e)ESP_LOGE(TAG,"SoftAP failed: %s",esp_err_to_name(e));
#endif
#if CONFIG_BK_ENABLE_KLINE
 if(xTaskCreate(ecu_task,"mikas",6144,NULL,6,NULL)!=pdPASS)ESP_LOGE(TAG,"Cannot start ECU task");
#else
 ESP_LOGW(TAG,"K-Line disabled: enable only after R2 bench checks");
#endif
 settings_queue=xQueueCreate(1,sizeof(bk_settings_t));
 if(settings_queue&&xTaskCreate(settings_task,"settings",3072,NULL,2,NULL)!=pdPASS){vQueueDelete(settings_queue);settings_queue=NULL;}
 if(display_status==ESP_OK){if(xTaskCreate(ui_task,"lvgl",8192,&settings,3,NULL)!=pdPASS)ESP_LOGE(TAG,"Cannot start UI task");}
 else ESP_LOGW(TAG,"Display unavailable: %s",esp_err_to_name(display_status));
}
