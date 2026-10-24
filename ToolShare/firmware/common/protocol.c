#include "protocol.h"
static void put16(uint8_t *p, uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t *p, uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint16_t get16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
uint16_t ts_crc16(const uint8_t *data,size_t n){uint16_t c=0xffff;for(size_t i=0;i<n;i++){c^=(uint16_t)((uint16_t)data[i]<<8);for(unsigned b=0;b<8;b++){uint32_t shifted=(uint32_t)c<<1;if((c&0x8000u)!=0u)shifted^=0x1021u;c=(uint16_t)shifted;}}return c;}
int ts_encode(const ts_message_t *m,uint8_t out[TS_FRAME_SIZE]){if(!m||!out||m->version!=TS_VERSION||m->type<1||m->type>3)return -1;out[0]=m->version;out[1]=m->type;put16(out+2,m->node_id);put32(out+4,m->sequence);put32(out+8,(uint32_t)m->value);put16(out+12,m->flags);put16(out+14,ts_crc16(out,14));return 0;}
int ts_decode(const uint8_t *f,size_t n,ts_message_t *m){if(!f||!m||n!=TS_FRAME_SIZE||f[0]!=TS_VERSION||f[1]<1||f[1]>3||get16(f+14)!=ts_crc16(f,14))return -1;m->version=f[0];m->type=f[1];m->node_id=get16(f+2);m->sequence=get32(f+4);m->value=(int32_t)get32(f+8);m->flags=get16(f+12);return 0;}
