#include "protocol.h"
uint16_t pp_crc16(const uint8_t *p,size_t n){uint16_t c=0xffff;for(size_t i=0;i<n;i++){c^=(uint16_t)p[i]<<8;for(unsigned b=0;b<8;b++)c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);}return c;}
static void put(uint8_t*p,uint32_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get(const uint8_t*p,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
size_t pp_encode(uint8_t*out,uint16_t node,uint32_t seq,uint8_t type,uint32_t value,uint32_t uptime){if(!out)return 0;out[0]=1;put(out+1,node,2);put(out+3,seq,4);out[7]=type;put(out+8,value,4);put(out+12,uptime,4);put(out+16,pp_crc16(out,16),2);return PP_FRAME_SIZE;}
int pp_decode(const uint8_t*in,size_t len,uint16_t*node,uint32_t*seq,uint8_t*type,uint32_t*value,uint32_t*uptime){if(!in||len!=PP_FRAME_SIZE||in[0]!=1||get(in+16,2)!=pp_crc16(in,16))return -1;if(node)*node=(uint16_t)get(in+1,2);if(seq)*seq=get(in+3,4);if(type)*type=in[7];if(value)*value=get(in+8,4);if(uptime)*uptime=get(in+12,4);return 0;}
