#include "protocol.h"
static void put32(uint8_t *p,uint32_t x){for(int i=0;i<4;i++)p[i]=(uint8_t)(x>>(8*i));}
static uint32_t get32(const uint8_t*p){uint32_t x=0;for(int i=0;i<4;i++)x|=(uint32_t)p[i]<<(8*i);return x;}
uint16_t lw_crc16(const uint8_t*d,size_t n){uint16_t c=0xffff;for(size_t i=0;i<n;i++){c^=(uint16_t)d[i]<<8;for(int b=0;b<8;b++)c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);}return c;}
int lw_encode(uint8_t o[LW_FRAME_LEN],uint8_t k,uint16_t id,uint32_t s,int32_t v){if(!o||k<1||k>3)return -1;o[0]=LW_VERSION;o[1]=k;o[2]=(uint8_t)id;o[3]=(uint8_t)(id>>8);put32(o+4,s);put32(o+8,(uint32_t)v);uint16_t c=lw_crc16(o,14);o[14]=(uint8_t)c;o[15]=(uint8_t)(c>>8);return 0;}
int lw_decode(const uint8_t*i,size_t n,uint8_t*k,uint16_t*id,uint32_t*s,int32_t*v){if(!i||n!=LW_FRAME_LEN||i[0]!=LW_VERSION||i[1]<1||i[1]>3||!k||!id||!s||!v)return -1;uint16_t c=(uint16_t)i[14]|((uint16_t)i[15]<<8);if(c!=lw_crc16(i,14))return -2;*k=i[1];*id=(uint16_t)i[2]|((uint16_t)i[3]<<8);*s=get32(i+4);*v=(int32_t)get32(i+8);return 0;}
