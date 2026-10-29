#include "protocol.h"
static void put32(uint8_t *p, uint32_t v) { for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint16_t crc16(const uint8_t *p, size_t n) { uint16_t c=0xffff; while(n--) { c^=(uint16_t)*p++<<8; for(unsigned i=0;i<8;i++) c=(uint16_t)((c&0x8000)?(c<<1)^0x1021:c<<1); } return c; }
int airshare_encode(uint8_t *o,size_t cap,uint32_t node,uint32_t seq,uint32_t up,uint16_t co2,uint16_t rh) { if(!o||cap<AIRSHARE_FRAME_LEN)return 0; o[0]=1;o[1]=1;put32(o+2,node);put32(o+6,seq);put32(o+10,up);o[14]=co2;o[15]=co2>>8;o[16]=rh;o[17]=rh>>8;uint16_t c=crc16(o,18);o[18]=c;o[19]=c>>8;return AIRSHARE_FRAME_LEN; }
int airshare_validate(const uint8_t *p,size_t n) { if(!p||n!=AIRSHARE_FRAME_LEN||p[0]!=1||p[1]!=1)return 0;uint16_t c=crc16(p,18);return p[18]==(uint8_t)c&&p[19]==(uint8_t)(c>>8); }
