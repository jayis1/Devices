#include "protocol.h"
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p, uint32_t v) { put16(p,(uint16_t)v); put16(p+2,(uint16_t)(v>>16)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1]<<8); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p) | ((uint32_t)get16(p+2)<<16); }
uint16_t fl_crc16(const uint8_t *data, size_t length) {
    uint16_t crc=0xffffu;
    for(size_t i=0;i<length;i++){ crc^=(uint16_t)data[i]<<8; for(unsigned b=0;b<8;b++) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); }
    return crc;
}
int fl_encode(const fl_telemetry_t *v, uint8_t out[FL_FRAME_LEN]) {
    if(!v||!out||v->version!=FL_VERSION) return -1;
    out[0]=v->version; out[1]=v->message_type; put16(out+2,v->node_id); put16(out+4,v->sequence); put32(out+6,v->unix_time_s);
    put16(out+10,(uint16_t)v->surface_temp_c_centi); put16(out+12,(uint16_t)v->air_temp_c_centi);
    put16(out+14,v->rh_permille); put16(out+16,v->wetness_u16); put16(out+18,v->battery_mv); out[20]=v->flags;
    put16(out+21,fl_crc16(out,21)); return 0;
}
int fl_decode(const uint8_t *f,size_t n,fl_telemetry_t *v) {
    if(!f||!v||n!=FL_FRAME_LEN||f[0]!=FL_VERSION||get16(f+21)!=fl_crc16(f,21)) return -1;
    v->version=f[0]; v->message_type=f[1]; v->node_id=get16(f+2); v->sequence=get16(f+4); v->unix_time_s=get32(f+6);
    v->surface_temp_c_centi=(int16_t)get16(f+10); v->air_temp_c_centi=(int16_t)get16(f+12);
    v->rh_permille=get16(f+14); v->wetness_u16=get16(f+16); v->battery_mv=get16(f+18); v->flags=f[20]; return 0;
}
