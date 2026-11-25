#include "protocol.h"
uint8_t mm_crc8(const uint8_t *p, size_t n) {
 uint8_t c=0;
 for(size_t i=0;i<n;i++){ c^=p[i]; for(unsigned b=0;b<8;b++) { uint8_t shifted=(uint8_t)(c<<1); c=(c&0x80u)?(uint8_t)(shifted^0x07u):shifted; } }
 return c;
}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int mm_encode(const mm_frame_t *f,uint8_t o[MM_FRAME_SIZE]){
 if(!f||!o||f->kind<1||f->kind>3)return 0;
 o[0]=MM_MAGIC;o[1]=MM_VERSION;o[2]=f->kind;put32(o+3,f->node_id);put32(o+7,f->sequence);put32(o+11,f->uptime_ms);
 o[15]=(uint8_t)f->value;o[16]=(uint8_t)((uint16_t)f->value>>8);o[17]=f->status;o[18]=0;o[19]=mm_crc8(o,19);return 1;
}
int mm_decode(const uint8_t *i,size_t n,mm_frame_t *f){
 if(!i||!f||n!=MM_FRAME_SIZE||i[0]!=MM_MAGIC||i[1]!=MM_VERSION||i[2]<1||i[2]>3||i[18]!=0||mm_crc8(i,19)!=i[19])return 0;
 f->kind=i[2];f->node_id=get32(i+3);f->sequence=get32(i+7);f->uptime_ms=get32(i+11);f->value=(int16_t)((uint16_t)i[15]|((uint16_t)i[16]<<8));f->status=i[17];return 1;
}
