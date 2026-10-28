#include "protocol.h"
static void put32(uint8_t*p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static uint32_t get32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
uint16_t pp_crc16(const uint8_t*d,size_t n){uint16_t c=0xffff;for(size_t i=0;i<n;i++){c^=(uint16_t)d[i]<<8;for(int b=0;b<8;b++)c=(uint16_t)((c&0x8000)?(c<<1)^0x1021:c<<1);}return c;}
int pp_encode(const pp_frame_t*f,uint8_t o[PP_FRAME_LEN]){if(!f||!o||f->version!=PP_VERSION||f->node_type<1||f->node_type>2)return -1;o[0]=f->version;o[1]=f->node_type;put32(o+2,f->node_id);put32(o+6,f->sequence);put32(o+10,f->age_ms);o[14]=f->value;o[15]=f->value>>8;o[16]=f->status;o[17]=0;uint16_t c=pp_crc16(o,18);o[18]=c;o[19]=c>>8;return 0;}
int pp_decode(const uint8_t*b,size_t n,pp_frame_t*f){if(!b||!f||n!=PP_FRAME_LEN||b[0]!=PP_VERSION||b[1]<1||b[1]>2||b[17]||pp_crc16(b,18)!=(uint16_t)(b[18]|(uint16_t)b[19]<<8))return -1;f->version=b[0];f->node_type=b[1];f->node_id=get32(b+2);f->sequence=get32(b+6);f->age_ms=get32(b+10);f->value=(uint16_t)(b[14]|(uint16_t)b[15]<<8);f->status=b[16];return 0;}
