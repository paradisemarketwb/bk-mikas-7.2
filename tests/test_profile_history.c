#include "bk_settings.h"
#include "bk_history.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 bk_settings_t s,t;bk_settings_defaults(&s);assert(bk_settings_valid(&s));
 char *json=bk_settings_to_json(&s);assert(json&&bk_settings_from_json(json,&t));
 if(argc>1){FILE *file=fopen(argv[1],"wb");assert(file);assert(fputs(json,file)>=0);fclose(file);}
 char *round=bk_settings_to_json(&t);assert(round&&!strcmp(json,round));free(round);
 cJSON *r=cJSON_Parse(json);assert(r);cJSON_AddNumberToObject(r,"version",3);char *bad=cJSON_PrintUnformatted(r);t.brightness=77;assert(!bk_settings_from_json(bad,&t)&&t.brightness==77);free(bad);cJSON_Delete(r);
 r=cJSON_Parse(json);cJSON *screens=cJSON_GetObjectItem(cJSON_GetObjectItem(r,"layout"),"screens");cJSON *widget=cJSON_GetArrayItem(cJSON_GetObjectItem(cJSON_GetArrayItem(screens,0),"widgets"),1);cJSON_ReplaceItemInObject(widget,"x",cJSON_CreateNumber(470));bad=cJSON_PrintUnformatted(r);assert(!bk_settings_from_json(bad,&t));free(bad);cJSON_Delete(r);free(json);
 assert(!bk_settings_from_json("{}",&t));assert(!bk_settings_from_json("{\"version\":3} trailing",&t));
 bk_history_t h;bk_history_init(&h,42);bk_snapshot_t data;bk_store_init(&data);data.connected=true;
 for(unsigned i=1;i<=200;i++){bk_store_reply(&data,TEL_RPM,(uint8_t[]){25},1,i*1000);bk_history_sample(&h,&data,i*1000);}
 assert(h.count==BK_LOG_POINTS&&h.observed_run_ms==199000);uint64_t runtime=h.observed_run_ms;data.connected=false;bk_history_sample(&h,&data,201000);assert(h.observed_run_ms==runtime);
 data.faults_valid=true;data.fault_count=1;data.faults[0]=53;data.faults[1]=0x20;bk_history_faults(&h,&data,201000);assert(h.event_count==1);bk_history_faults(&h,&data,202000);assert(h.event_count==1);
 data.fault_count=0;bk_history_faults(&h,&data,203000);assert(h.event_count==2&&h.events[1].present==0);
 for(unsigned i=0;i<100;i++){data.fault_count=1;data.faults[0]=(uint8_t)i;bk_history_faults(&h,&data,204000+i);}assert(h.event_count==BK_HISTORY_EVENTS);
 puts("profile JSON atomic import/roundtrip/duplicate and geometry rejection; bounded history and DTC events: OK");
}
