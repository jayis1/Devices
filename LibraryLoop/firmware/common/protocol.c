#include "protocol.h"
static void put16(uint8_t *p, uint16_t x) { p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1]<<8)); }
static void put32(uint8_t *p, uint32_t x) { p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); p[2]=(uint8_t)(x>>16); p[3]=(uint8_t)(x>>24); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
uint16_t ll_crc16(const uint8_t *data, size_t length) {
    uint16_t crc=0xffffu;
    if (data == NULL) return 0u;
    for (size_t i=0; i<length; ++i) { crc ^= (uint16_t)data[i]<<8; for (unsigned b=0; b<8; ++b) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); }
    return crc;
}
int ll_encode(uint8_t out[LL_FRAME_SIZE], const ll_telemetry_t *m) {
    if (out==NULL || m==NULL || m->kind>2u || (m->occupied_mask & (uint16_t)~m->valid_mask)!=0u) return -1;
    out[0]=LL_PROTOCOL_VERSION; out[1]=m->kind; out[2]=m->node_id; out[3]=0u;
    put16(out+4,m->sequence); put16(out+6,m->occupied_mask); put16(out+8,m->valid_mask); put32(out+10,m->uptime_s);
    out[14]=out[15]=out[16]=out[17]=0u; put16(out+18,ll_crc16(out,18u)); return 0;
}
int ll_decode(ll_telemetry_t *m, const uint8_t *f, size_t n) {
    if (m==NULL || f==NULL || n!=LL_FRAME_SIZE || f[0]!=LL_PROTOCOL_VERSION || f[1]>2u || f[3]!=0u) return -1;
    if (f[14]||f[15]||f[16]||f[17]||get16(f+18)!=ll_crc16(f,18u)) return -1;
    m->kind=f[1]; m->node_id=f[2]; m->sequence=get16(f+4); m->occupied_mask=get16(f+6); m->valid_mask=get16(f+8); m->uptime_s=get32(f+10);
    return (m->occupied_mask & (uint16_t)~m->valid_mask)==0u ? 0 : -1;
}
