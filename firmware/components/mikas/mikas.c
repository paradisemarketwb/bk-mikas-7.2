#include "mikas.h"
size_t mikas_encode(const uint8_t *b,size_t n,uint8_t *w,size_t cap) {
    if (!b || !w || !n || n>MIKAS_MAX_PAYLOAD) return 0;
    uint8_t sum=0; size_t needed=1;
    for(size_t i=0;i<n;i++){sum+=b[i];needed+=(b[i]==0x0d||b[i]==0x40)?2:1;}
    uint8_t check=(uint8_t)(0-sum); needed+=(check==0x0d||check==0x40)?2:1;
    if(cap<needed)return 0;
    size_t at=0;
    for(size_t i=0;i<=n;i++) {
        uint8_t v=i<n?b[i]:check;
        if(v==0x0d||v==0x40){w[at++]=0x40;w[at++]=v==0x0d?0xcd:0;}
        else w[at++]=v;
    }
    w[at++]=0x0d; return at;
}
bool mikas_decode(const uint8_t *w,size_t n,uint8_t *b,size_t cap,size_t *out) {
    if(out)*out=0;
    if(!w||!b||!out||n<3||w[n-1]!=0x0d||n>MIKAS_MAX_WIRE)return false;
    uint8_t tmp[MIKAS_MAX_PAYLOAD+1],sum=0;size_t at=0;
    for(size_t i=0;i<n-1;i++) {
        uint8_t v=w[i]; if(v==0x0d)return false;
        if(v==0x40){if(++i>=n-1)return false;v=w[i];if(v==0xcd)v=0x0d;else if(v==0)v=0x40;else return false;}
        if(at>=sizeof(tmp))return false;
        tmp[at++]=v;sum+=v;
    }
    if(at<2||sum||at-1>cap)return false;
    for(size_t i=0;i<at-1;i++)b[i]=tmp[i];
    *out=at-1;return true;
}
bool mikas_read_allowed(const uint8_t *b,size_t n) {
    if(!b)return false;
    if(n==1)return b[0]==1||b[0]==2||(b[0]>=0x51&&b[0]<=0x58);
    return n==2&&b[0]==0x61;
}
bool mikas_faults_valid(const uint8_t *b,size_t n) {
    if(!b||!n||n!=1+2*(size_t)b[0])return false;
    for(size_t i=2;i<n;i+=2)if(b[i]&0x1f)return false;
    return true;
}
