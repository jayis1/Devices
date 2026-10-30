#include "protocol.h"
uint16_t bp_crc16(const uint8_t *d,size_t n){uint16_t c=0xffff;size_t i;unsigned b;for(i=0;i<n;i++){c^=(uint16_t)d[i]<<8;for(b=0;b<8;b++)c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);}return c;}
static void p16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static uint16_t g16(const uint8_t*p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void p32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static uint32_t g32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
int bp_encode(uint8_t n,uint16_t s,uint32_t t,uint8_t f,uint16_t v,int16_t c,uint8_t o[BP_FRAME_LEN]){if(!o)return -1;o[0]=0xb5;o[1]=1;o[2]=1;o[3]=n;p16(o+4,s);p32(o+6,t);o[10]=f;p16(o+11,v);p16(o+13,(uint16_t)c);o[15]=0;p16(o+16,bp_crc16(o,16));return 0;}
int bp_decode(const uint8_t*i,size_t z,uint8_t*n,uint16_t*s,uint32_t*t,uint8_t*f,uint16_t*v,int16_t*c){if(!i||z!=BP_FRAME_LEN||i[0]!=0xb5||i[1]!=1||i[2]!=1||i[15]!=0||g16(i+16)!=bp_crc16(i,16))return -1;if(n)*n=i[3];if(s)*s=g16(i+4);if(t)*t=g32(i+6);if(f)*f=i[10];if(v)*v=g16(i+11);if(c)*c=(int16_t)g16(i+13);return 0;}
