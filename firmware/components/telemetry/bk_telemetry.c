#include "bk_telemetry.h"
#include <string.h>
const bk_field_t bk_fields[TEL_METRIC_COUNT]={
 {0x29,1,40,0,false,200},{0x1a,1,1,-40,false,1000},{0x1e,1,.1,0,false,1000},
 {0x20,1,1,0,false,500},{0x1c,1,1,-40,false,2000},{0x26,1,.5,0,true,1000},
 {0x21,2,0,0,false,2000},/* divisor unconfirmed: retain NEEDS_DATA */
 {0x40,2,.1,0,false,2000},{0x3f,2,.008,0,false,1000},
 {0x4d,1,1,0,false,2000},{0x4e,1,1,0,false,2000}};
void bk_store_init(bk_snapshot_t *s){memset(s,0,sizeof(*s));}
bool bk_store_reply(bk_snapshot_t *s,bk_tel_metric_t m,const uint8_t *b,unsigned n,uint64_t now){
 if(!s||!b||m>=TEL_METRIC_COUNT||m<0||n!=bk_fields[m].width)return false;
 const bk_field_t *f=&bk_fields[m];int raw=b[0];
 if(n==2)raw|=(int)b[1]<<8;
 if(f->is_signed)raw=(int8_t)b[0];
 s->samples[m]=(bk_sample_t){raw*f->scale+f->offset,now,m==TEL_AIR?TEL_NEEDS_DATA:TEL_VALID};return true;
}
bk_sample_t bk_store_get(const bk_snapshot_t *s,bk_tel_metric_t m,uint64_t now){
 if(!s||m<0||m>=TEL_METRIC_COUNT)return (bk_sample_t){.state=TEL_UNAVAILABLE};
 bk_sample_t v=s->samples[m];
 if(!s->connected)v.state=TEL_NO_ECU;
 else if(v.state==TEL_VALID&&now-v.at_ms>3000)v.state=TEL_STALE;
 return v;
}
