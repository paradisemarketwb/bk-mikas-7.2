#include "bk_settings.h"
#include "nvs.h"
#include <string.h>
static bool valid(const bk_settings_t *s){for(unsigned i=0;i<5;i++)if(s->selected[i]>=15)return false;return s->version==2&&s->screen<10&&s->brightness>=5&&s->brightness<=100&&s->dark<=1;}
esp_err_t bk_settings_load(bk_settings_t *s){
 *s=(bk_settings_t){.version=2,.screen=1,.brightness=80,.dark=1,.selected={8,1,4,5,10}};nvs_handle_t h;
 esp_err_t e=nvs_open("bk_settings",NVS_READONLY,&h);if(e==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;if(e)return e;
 bk_settings_t v;size_t n=sizeof(v);e=nvs_get_blob(h,"ui",&v,&n);nvs_close(h);
 if(e==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
 if(e)return e;
 if(n!=sizeof(v)||!valid(&v))return ESP_ERR_INVALID_STATE;
 *s=v;return ESP_OK;
}
esp_err_t bk_settings_save(const bk_settings_t *s){
 if(!s||!valid(s))return ESP_ERR_INVALID_ARG;
 nvs_handle_t h;esp_err_t e=nvs_open("bk_settings",NVS_READWRITE,&h);if(e)return e;
 bk_settings_t old;size_t n=sizeof(old);
 if(nvs_get_blob(h,"ui",&old,&n)==ESP_OK&&n==sizeof(old)&&memcmp(&old,s,n)==0){nvs_close(h);return ESP_OK;}
 e=nvs_set_blob(h,"ui",s,sizeof(*s));if(!e)e=nvs_commit(h);nvs_close(h);return e;
}
