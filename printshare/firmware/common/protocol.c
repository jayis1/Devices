#include "protocol.h"
static void put16(uint8_t *p, uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t *p, uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint16_t get16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
uint16_t ps_crc16(const uint8_t *data,size_t len){uint16_t crc=0xffff;for(size_t i=0;i<len;i++){crc^=(uint16_t)data[i]<<8;for(int b=0;b<8;b++)crc=(crc&0x8000)?(uint16_t)((crc<<1)^0x1021):(uint16_t)(crc<<1);}return crc;}
int ps_encode(uint8_t o[PS_FRAME_SIZE],uint8_t t,uint16_t id,uint16_t seq,uint32_t up,uint8_t st,uint16_t rh,int16_t tc,uint16_t mv){if(!o||(t!=PS_TYPE_PRINTER&&t!=PS_TYPE_FILAMENT))return -1;o[0]=PS_VERSION;o[1]=t;put16(o+2,id);put16(o+4,seq);put32(o+6,up);o[10]=st;put16(o+11,rh);put16(o+13,(uint16_t)tc);put16(o+15,mv);put16(o+16,ps_crc16(o,16));return 0;}
int ps_validate(const uint8_t *f,size_t n){if(!f||n!=PS_FRAME_SIZE||f[0]!=PS_VERSION||(f[1]!=PS_TYPE_PRINTER&&f[1]!=PS_TYPE_FILAMENT))return 0;return get16(f+16)==ps_crc16(f,16);}
