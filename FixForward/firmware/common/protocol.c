#include "protocol.h"
uint16_t ff_crc16(const uint8_t *p,size_t n){uint16_t c=0xffff;for(size_t i=0;i<n;i++){c^=(uint16_t)p[i]<<8;for(int b=0;b<8;b++)c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);}return c;}
int ff_valid(const ff_message*m){return m && m->version==FF_VERSION && m->node_id!=0 && m->type>=1 && m->type<=3;}
