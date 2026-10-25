#include "protocol.h"
uint16_t cr_crc16(const uint8_t *p, size_t n) {
    uint16_t crc=0xffffu;
    while(n--) { crc ^= (uint16_t)*p++ << 8; for(unsigned i=0;i<8;i++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); }
    return crc;
}
int cr_encode(uint8_t *o,size_t cap,uint8_t t,uint16_t id,uint16_t seq,const uint8_t *p,uint8_t n,size_t *w) {
    if(!o||!w||n>CR_MAX_PAYLOAD||(!p&&n)||cap<CR_HEADER_SIZE+n+CR_CRC_SIZE||t<CR_TELEMETRY||t>CR_ACK)return -1;
    o[0]=CR_VERSION;o[1]=t;o[2]=(uint8_t)id;o[3]=(uint8_t)(id>>8);o[4]=(uint8_t)seq;o[5]=(uint8_t)(seq>>8);o[6]=n;
    for(unsigned i=0;i<n;i++) o[7+i]=p[i];
    uint16_t c=cr_crc16(o,7+n);
    o[7+n]=(uint8_t)c;
    o[8+n]=(uint8_t)(c>>8);
    *w=9+n;
    return 0;
}
int cr_decode(const uint8_t *f,size_t n,uint8_t *t,uint16_t *id,uint16_t *seq,const uint8_t **p,uint8_t *plen) {
    if(!f||n<9||!t||!id||!seq||!p||!plen||f[0]!=CR_VERSION||f[1]<CR_TELEMETRY||f[1]>CR_ACK||f[6]>CR_MAX_PAYLOAD||n!=9u+f[6])return -1;
    uint16_t c=(uint16_t)f[n-2]|(uint16_t)f[n-1]<<8;if(c!=cr_crc16(f,n-2))return -2;
    *t=f[1];*id=(uint16_t)f[2]|(uint16_t)f[3]<<8;*seq=(uint16_t)f[4]|(uint16_t)f[5]<<8;*plen=f[6];*p=f+7;return 0;
}
