#include "protocol.h"
uint16_t cr_crc16(const uint8_t *p, size_t n) {
    uint16_t crc=0xffffu;
    for(size_t i=0;i<n;i++){ crc^=(uint16_t)p[i]<<8; for(unsigned b=0;b<8;b++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); }
    return crc;
}
int cr_encode(uint8_t type,uint16_t node,uint32_t seq,const uint8_t *payload,uint8_t plen,uint8_t *out,size_t cap,size_t *written){
    size_t n=(size_t)10+plen;
    if(!out||!written||(!payload&&plen)||plen>CR_FRAME_MAX-10u||cap<n)return 0;
    out[0]=CR_VERSION;out[1]=type;out[2]=(uint8_t)(node>>8);out[3]=(uint8_t)node;
    out[4]=(uint8_t)(seq>>24);out[5]=(uint8_t)(seq>>16);out[6]=(uint8_t)(seq>>8);out[7]=(uint8_t)seq;out[8]=plen;
    for(uint8_t i=0;i<plen;i++)out[9u+i]=payload[i];
    uint16_t c=cr_crc16(out,n-2u);out[n-2u]=(uint8_t)(c>>8);out[n-1u]=(uint8_t)c;*written=n;return 1;
}
int cr_validate(const uint8_t *f,size_t n){
    if(!f||n<10u||n>CR_FRAME_MAX||f[0]!=CR_VERSION||n!=(size_t)10u+f[8])return 0;
    uint16_t got=(uint16_t)(((uint16_t)f[n-2u]<<8)|f[n-1u]);return got==cr_crc16(f,n-2u);
}
