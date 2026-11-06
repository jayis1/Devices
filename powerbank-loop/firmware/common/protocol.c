#include "protocol.h"
#include <string.h>
#define MAGIC0 0x50u
#define MAGIC1 0x42u
static uint16_t crc16(const uint8_t *p, size_t n) { uint16_t c=0xffffu; for(size_t i=0;i<n;i++){ c^=(uint16_t)p[i]<<8; for(int b=0;b<8;b++) c=(c&0x8000u)?(uint16_t)((c<<1)^0x1021u):(uint16_t)(c<<1); } return c; }
size_t pbl_encode(const pbl_frame_t *f, uint8_t *o, size_t cap) {
 if(!f || !o || f->len>PBL_MAX_PAYLOAD) return 0;
 size_t n=12u+f->len;
 if(cap<n) return 0;
 o[0]=MAGIC0; o[1]=MAGIC1; o[2]=1u; o[3]=f->node;
 o[4]=(uint8_t)f->seq; o[5]=(uint8_t)(f->seq>>8);
 o[6]=(uint8_t)(f->seq>>16); o[7]=(uint8_t)(f->seq>>24);
 o[8]=f->kind; o[9]=f->len;
 if(f->len) memcpy(o+10,f->payload,f->len);
 uint16_t c=crc16(o,10u+f->len);
 o[10u+f->len]=(uint8_t)c; o[11u+f->len]=(uint8_t)(c>>8);
 return n;
}
int pbl_decode(const uint8_t *d,size_t n,pbl_frame_t *f) {
 if(!d||!f||n<12u||d[0]!=MAGIC0||d[1]!=MAGIC1||d[2]!=1u||d[9]>PBL_MAX_PAYLOAD||n!=12u+d[9]) return 0;
 uint16_t got=(uint16_t)d[n-2]|((uint16_t)d[n-1]<<8);if(crc16(d,n-2)!=got)return 0;
 f->node=d[3];f->seq=(uint32_t)d[4]|((uint32_t)d[5]<<8)|((uint32_t)d[6]<<16)|((uint32_t)d[7]<<24);f->kind=d[8];f->len=d[9];if(f->len)memcpy(f->payload,d+10,f->len);return 1;
}
