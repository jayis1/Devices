#include "protocol.h"
static void put32(uint8_t *p, uint32_t x) { for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(x>>(8*i)); }
static uint32_t get32(const uint8_t *p) { uint32_t x=0; for (unsigned i=0;i<4;i++) x|=(uint32_t)p[i]<<(8*i); return x; }
uint16_t labsignal_crc16(const uint8_t *p, size_t n) { uint16_t c=0xffff; for(size_t i=0;i<n;i++){ c^=(uint16_t)p[i]<<8; for(unsigned b=0;b<8;b++) c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1); } return c; }
int labsignal_encode(const labsignal_message_t *m, uint8_t o[LABSIGNAL_FRAME_SIZE]) { if(!m||!o||m->kind>3) return -1; o[0]=LABSIGNAL_VERSION;o[1]=m->kind;put32(o+2,m->sequence);put32(o+6,(uint32_t)m->value_milli);uint16_t c=labsignal_crc16(o,LABSIGNAL_BODY_SIZE);o[10]=(uint8_t)c;o[11]=(uint8_t)(c>>8);return 0; }
int labsignal_decode(const uint8_t *i,size_t n,labsignal_message_t *m) { if(!i||!m||n!=LABSIGNAL_FRAME_SIZE||i[0]!=LABSIGNAL_VERSION||i[1]>3)return -1;uint16_t c=(uint16_t)i[10]|(uint16_t)i[11]<<8;if(c!=labsignal_crc16(i,LABSIGNAL_BODY_SIZE))return -1;m->kind=i[1];m->sequence=get32(i+2);m->value_milli=(int32_t)get32(i+6);return 0; }
