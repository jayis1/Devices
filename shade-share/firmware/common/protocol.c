#include "protocol.h"
uint16_t ss_crc16(const uint8_t *d, size_t n) { uint16_t c=0xffff; for(size_t i=0;i<n;i++){c^=(uint16_t)d[i]<<8;for(unsigned b=0;b<8;b++)c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);}return c; }
static void put16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
int ss_encode(uint8_t o[SS_FRAME_LEN],uint32_t id,uint16_t seq,uint8_t k,int16_t v,uint16_t mv){if(!o)return -1;o[0]=SS_VERSION;o[1]=id>>24;o[2]=id>>16;o[3]=id>>8;o[4]=id;put16(o+5,seq);o[7]=k;put16(o+8,(uint16_t)v);put16(o+10,mv);put16(o+12,ss_crc16(o,12));return 0;}
int ss_decode(const uint8_t*i,size_t n,uint32_t*id,uint16_t*s,uint8_t*k,int16_t*v,uint16_t*mv){if(!i||n!=SS_FRAME_LEN||!id||!s||!k||!v||!mv||i[0]!=SS_VERSION||ss_crc16(i,12)!=((uint16_t)i[12]<<8|i[13]))return -1;*id=(uint32_t)i[1]<<24|(uint32_t)i[2]<<16|(uint32_t)i[3]<<8|i[4];*s=(uint16_t)i[5]<<8|i[6];*k=i[7];*v=(int16_t)((uint16_t)i[8]<<8|i[9]);*mv=(uint16_t)i[10]<<8|i[11];return 0;}
