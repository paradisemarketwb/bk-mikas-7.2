#include "bk_wifi.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
static char ssid[32],password[33];
static bk_snapshot_fn snapshot;
const char *bk_wifi_ssid(void){return ssid;}
const char *bk_wifi_password(void){return password;}
static esp_err_t index_get(httpd_req_t *r){
 httpd_resp_set_type(r,"text/html; charset=utf-8");
 httpd_resp_set_hdr(r,"Cache-Control","no-store");
 return httpd_resp_sendstr(r,"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'><title>HUNTER</title><h1>HUNTER · Микас 7.2</h1><p>Локальная телеметрия. Статусы 1: доступно, 2: устарело, 3: нет связи, 5: нужны данные.</p><pre id=data></pre><script>setInterval(async()=>{try{data.textContent=JSON.stringify(await(await fetch('/api/telemetry')).json(),null,2)}catch(e){data.textContent='Нет связи с прибором'}},1000)</script>");
}
static esp_err_t data_get(httpd_req_t *r){
 bk_snapshot_t s;snapshot(&s);uint64_t now=esp_timer_get_time()/1000;
 cJSON *root=cJSON_CreateObject();if(!root)return ESP_ERR_NO_MEM;
 cJSON_AddBoolToObject(root,"connected",s.connected);
 cJSON *a=cJSON_AddArrayToObject(root,"metrics");
 static const char *keys[]={"rpm","coolant","volts","throttle","intake","advance","air","fuel_lph","injection_ms","lambda1_raw","lambda2_raw"};
 for(unsigned i=0;i<TEL_METRIC_COUNT;i++){
  bk_sample_t v=bk_store_get(&s,i,now);cJSON *m=cJSON_CreateObject();
  cJSON_AddStringToObject(m,"key",keys[i]);cJSON_AddNumberToObject(m,"state",v.state);
  if(v.state==TEL_VALID)cJSON_AddNumberToObject(m,"value",v.value);else cJSON_AddNullToObject(m,"value");
  if(v.state==TEL_VALID||v.state==TEL_STALE)cJSON_AddNumberToObject(m,"age_ms",now-v.at_ms);
  cJSON_AddItemToArray(a,m);
 }
 char *body=cJSON_PrintUnformatted(root);cJSON_Delete(root);if(!body)return ESP_ERR_NO_MEM;
 httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
 esp_err_t e=httpd_resp_sendstr(r,body);cJSON_free(body);return e;
}
esp_err_t bk_wifi_start(bk_snapshot_fn fn){
 if(!fn)return ESP_ERR_INVALID_ARG;
 snapshot=fn;nvs_handle_t h;esp_err_t e=nvs_open("bk_wifi",NVS_READWRITE,&h);if(e)return e;
 size_t size=sizeof(password);e=nvs_get_str(h,"password",password,&size);
 if(e==ESP_ERR_NVS_NOT_FOUND){uint8_t random[16];esp_fill_random(random,sizeof(random));for(unsigned i=0;i<sizeof(random);i++)snprintf(password+i*2,3,"%02x",random[i]);e=nvs_set_str(h,"password",password);if(!e)e=nvs_commit(h);}
 nvs_close(h);if(e)return e;
 uint8_t mac[6];
 e=esp_netif_init();if(e)return e;
 e=esp_event_loop_create_default();if(e)return e;
 if(!esp_netif_create_default_wifi_ap())return ESP_ERR_NO_MEM;
 wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();e=esp_wifi_init(&init);if(e)return e;
 e=esp_wifi_get_mac(WIFI_IF_AP,mac);if(e)return e;
 snprintf(ssid,sizeof(ssid),"HUNTER-%02X%02X%02X",mac[3],mac[4],mac[5]);
 wifi_config_t cfg={.ap={.channel=6,.max_connection=2,.authmode=WIFI_AUTH_WPA2_PSK,.pmf_cfg={.capable=true}}};
 memcpy(cfg.ap.ssid,ssid,strlen(ssid));cfg.ap.ssid_len=strlen(ssid);memcpy(cfg.ap.password,password,strlen(password));
 e=esp_wifi_set_mode(WIFI_MODE_AP);if(e)return e;e=esp_wifi_set_config(WIFI_IF_AP,&cfg);if(e)return e;e=esp_wifi_start();if(e)return e;
 httpd_config_t hc=HTTPD_DEFAULT_CONFIG();hc.max_uri_handlers=2;hc.stack_size=6144;httpd_handle_t server;
 e=httpd_start(&server,&hc);if(e)return e;
 httpd_uri_t home={.uri="/",.method=HTTP_GET,.handler=index_get};httpd_uri_t data={.uri="/api/telemetry",.method=HTTP_GET,.handler=data_get};
 e=httpd_register_uri_handler(server,&home);if(e)return e;return httpd_register_uri_handler(server,&data);
}
