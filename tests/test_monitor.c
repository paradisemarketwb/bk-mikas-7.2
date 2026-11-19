#include "mikas.h"
#include "bk_telemetry.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 uint8_t w[MIKAS_MAX_WIRE],b[256];size_t out;
 const uint8_t cmd[]={0x61,0x29},expected[]={0x61,0x29,0x76,0x0d};
 assert(mikas_encode(cmd,2,w,sizeof(w))==sizeof(expected));assert(!memcmp(w,expected,sizeof(expected)));
 for(unsigned i=0;i<256;i++){
  uint8_t raw[]={i,0x0d,0x40};size_t n=mikas_encode(raw,sizeof(raw),w,sizeof(w));
  assert(n&&mikas_decode(w,n,b,sizeof(b),&out)&&out==sizeof(raw)&&!memcmp(raw,b,out));
  w[n-2]^=1;assert(!mikas_decode(w,n,b,sizeof(b),&out));
 }
 assert(!mikas_decode((uint8_t[]){0x40,0x23,0x0d},3,b,sizeof(b),&out));
 assert(!mikas_decode((uint8_t[]){1,0x40,0x0d},3,b,sizeof(b),&out));
 assert(!mikas_decode((uint8_t[]){1,0x0d,0xff,0x0d},4,b,sizeof(b),&out));
 assert(!mikas_read_allowed((uint8_t[]){0x62,0x0e,8},3));
 assert(!mikas_read_allowed((uint8_t[]){0x03},1));
 assert(mikas_faults_valid((uint8_t[]){1,53,0x20},3));
 assert(!mikas_faults_valid((uint8_t[]){1,53,1},3));
 bk_snapshot_t s;bk_store_init(&s);assert(bk_store_get(&s,TEL_RPM,100).state==TEL_NO_ECU);
 s.connected=true;assert(bk_store_get(&s,TEL_RPM,100).state==TEL_UNAVAILABLE);
 assert(bk_store_reply(&s,TEL_RPM,(uint8_t[]){20},1,100));
 assert(bk_store_get(&s,TEL_RPM,100).value==800);
 assert(bk_store_get(&s,TEL_RPM,3200).state==TEL_STALE);
 assert(!bk_store_reply(&s,TEL_INJECTION,(uint8_t[]){1},1,100));
 assert(bk_store_reply(&s,TEL_AIR,(uint8_t[]){240,0},2,100));
 assert(bk_store_get(&s,TEL_AIR,100).state==TEL_NEEDS_DATA);
 puts("monitor codec, read-only boundary, field validity: OK");
}
