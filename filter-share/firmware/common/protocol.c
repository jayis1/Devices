#include "protocol.h"
static void put32(uint8_t *p, uint32_t v) { for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t get32(const uint8_t *p) { uint32_t v=0; for (unsigned i=0;i<4;i++) v|=(uint32_t)p[i]<<(8*i); return v; }
uint16_t fs_crc16(const uint8_t *p, size_t n) {
  uint16_t c=0xffff;
  while(n--) { c^=(uint16_t)*p++<<8; for(unsigned i=0;i<8;i++) c=(uint16_t)((c&0x8000)?(c<<1)^0x1021:c<<1); }
  return c;
}
int fs_encode(uint8_t o[FS_FRAME_LEN], uint8_t t, uint32_t id, uint32_t s, uint32_t u, int32_t a, int32_t b) {
  if(!o || t<1 || t>3) return -1;
  o[0]=0x53; o[1]=0x46; o[2]=FS_VERSION; o[3]=t; put32(o+4,id); put32(o+8,s); put32(o+12,u); put32(o+16,(uint32_t)a); put32(o+20,(uint32_t)b);
  uint16_t c=fs_crc16(o,24); o[24]=(uint8_t)c; o[25]=(uint8_t)(c>>8); return 0;
}
int fs_decode(const uint8_t *i,size_t n,uint8_t *t,uint32_t *id,uint32_t *s,uint32_t *u,int32_t *a,int32_t *b) {
  if(!i||!t||!id||!s||!u||!a||!b||n!=FS_FRAME_LEN||i[0]!=0x53||i[1]!=0x46||i[2]!=FS_VERSION||i[3]<1||i[3]>3) return -1;
  if(((uint16_t)i[24]|((uint16_t)i[25]<<8))!=fs_crc16(i,24)) return -2;
  *t=i[3]; *id=get32(i+4); *s=get32(i+8); *u=get32(i+12); *a=(int32_t)get32(i+16); *b=(int32_t)get32(i+20); return 0;
}
