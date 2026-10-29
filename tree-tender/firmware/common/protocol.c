#include "protocol.h"

static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1]<<8); }
static void put32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
uint16_t tt_crc16(const uint8_t *data, size_t len) {
    uint16_t crc=0xffffu;
    for(size_t i=0;i<len;i++){ crc^=(uint16_t)data[i]<<8; for(unsigned b=0;b<8;b++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); }
    return crc;
}
size_t tt_encode(const tt_observation *o, uint8_t *out, size_t cap) {
    if(!o || !out || cap<14 || o->version!=TT_VERSION) return 0;
    out[0]=o->version; put16(out+1,o->node_id); put16(out+3,o->sequence); out[5]=o->kind;
    put16(out+6,(uint16_t)o->value_x100); put32(out+8,o->timestamp_s); put16(out+12,tt_crc16(out,12)); return 14;
}
int tt_decode(const uint8_t *d, size_t n, tt_observation *o) {
    if(!d || !o || n!=14 || d[0]!=TT_VERSION || get16(d+12)!=tt_crc16(d,12)) return 0;
    o->version=d[0]; o->node_id=get16(d+1); o->sequence=get16(d+3); o->kind=d[5];
    o->value_x100=(int16_t)get16(d+6); o->timestamp_s=get32(d+8); return 1;
}
