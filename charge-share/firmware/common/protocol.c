#include "protocol.h"
static void put32(uint8_t *p, uint32_t x){ for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(x>>(8*i)); }
static uint16_t crc16(const uint8_t *p,size_t n){ uint16_t c=0xffff; while(n--){ c^=(uint16_t)*p++<<8; for(int i=0;i<8;i++) c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1); } return c; }
int charges_encode(uint8_t *o,size_t cap,uint32_t node,uint32_t seq,uint32_t ms,uint16_t mv,uint16_t ma){ if(!o||cap<CHARGESHARE_FRAME_LEN)return 0; o[0]=1;o[1]=1;put32(o+2,node);put32(o+6,seq);put32(o+10,ms);o[14]=mv;o[15]=mv>>8;o[16]=ma;o[17]=ma>>8;uint16_t c=crc16(o,18);o[18]=c;o[19]=c>>8;return CHARGESHARE_FRAME_LEN; }
int charges_validate(const uint8_t *p,size_t n){ if(!p||n!=CHARGESHARE_FRAME_LEN||p[0]!=1||p[1]!=1)return 0; return crc16(p,18)==(uint16_t)(p[18]|((uint16_t)p[19]<<8)); }
