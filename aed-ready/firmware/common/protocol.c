#include "protocol.h"
uint16_t aedready_crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffffu;
    for (size_t i=0;i<n;i++) { crc ^= (uint16_t)p[i] << 8; for (unsigned b=0;b<8;b++) crc = (crc & 0x8000u) ? (uint16_t)((crc<<1)^0x1021u) : (uint16_t)(crc<<1); }
    return crc;
}
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put32(uint8_t *p,uint32_t v){put16(p,(uint16_t)v);put16(p+2,(uint16_t)(v>>16));}
static uint16_t get16(const uint8_t*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t get32(const uint8_t*p){return (uint32_t)get16(p)|((uint32_t)get16(p+2)<<16);}
size_t aedready_encode(const aedready_status_t*m,uint8_t*out,size_t cap){if(!m||!out||cap<AEDREADY_FRAME_SIZE)return 0;out[0]=m->version;out[1]=m->kind;put16(out+2,m->node_id);put32(out+4,m->sequence);put32(out+8,m->uptime_s);put16(out+12,(uint16_t)m->temperature_centi_c);put16(out+14,m->battery_mv);put16(out+16,m->flags);put16(out+18,0);put32(out+20,0);put16(out+18,aedready_crc16(out,18));return AEDREADY_FRAME_SIZE;}
int aedready_decode(const uint8_t*d,size_t n,aedready_status_t*m){if(!d||!m||n!=AEDREADY_FRAME_SIZE||d[0]!=AEDREADY_VERSION||d[1]!=AEDREADY_KIND_STATUS||get16(d+18)!=aedready_crc16(d,18)||get16(d+20)||get16(d+22))return 0;m->version=d[0];m->kind=d[1];m->node_id=get16(d+2);m->sequence=get32(d+4);m->uptime_s=get32(d+8);m->temperature_centi_c=(int16_t)get16(d+12);m->battery_mv=get16(d+14);m->flags=get16(d+16);m->crc16=get16(d+18);return 1;}
