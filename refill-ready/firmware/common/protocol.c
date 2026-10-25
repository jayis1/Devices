#include "protocol.h"
static void put16(uint8_t *p, uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static uint16_t get16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static void put32(uint8_t *p, uint32_t v){put16(p,(uint16_t)v);put16(p+2,(uint16_t)(v>>16));}
static uint32_t get32(const uint8_t *p){return (uint32_t)get16(p)|((uint32_t)get16(p+2)<<16);}
uint16_t rr_crc16(const uint8_t *d,uint32_t n){uint16_t c=0xffff;for(uint32_t i=0;i<n;i++){c^=(uint16_t)d[i]<<8;for(unsigned b=0;b<8;b++)c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);}return c;}
int rr_encode(const struct rr_frame *f,uint8_t o[RR_FRAME_SIZE]){if(!f||!o||f->version!=RR_VERSION)return -1;o[0]=f->version;o[1]=f->type;put16(o+2,f->node_id);put16(o+4,f->sequence);put32(o+6,f->uptime_s);put16(o+10,(uint16_t)f->value_a);put16(o+12,(uint16_t)f->value_b);put16(o+14,rr_crc16(o,14));return 0;}
int rr_decode(const uint8_t i[RR_FRAME_SIZE],struct rr_frame *f){if(!i||!f||i[0]!=RR_VERSION||get16(i+14)!=rr_crc16(i,14))return -1;f->version=i[0];f->type=i[1];f->node_id=get16(i+2);f->sequence=get16(i+4);f->uptime_s=get32(i+6);f->value_a=(int16_t)get16(i+10);f->value_b=(int16_t)get16(i+12);f->crc=get16(i+14);return 0;}
