#include "protocol.h"
uint16_t pp_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu; size_t i; unsigned bit;
    for (i=0;i<len;++i) { crc^=(uint16_t)data[i]<<8; for(bit=0;bit<8;++bit) crc=(crc&0x8000u)?(uint16_t)((crc<<1)^0x1021u):(uint16_t)(crc<<1); }
    return crc;
}
static uint16_t u16le(const uint8_t *p) { return (uint16_t)p[0]|((uint16_t)p[1]<<8); }
int pp_frame_valid(const uint8_t *f,size_t n) {
    uint16_t payload;
    if(!f||n<PP_FRAME_OVERHEAD||f[0]!=0x50u||f[1]!=0x50u||f[2]!=1u) return 0;
    payload=u16le(&f[8]); if(payload>PP_MAX_PAYLOAD||n!=(size_t)PP_FRAME_OVERHEAD+payload) return 0;
    return pp_crc16(f,n-2u)==u16le(&f[n-2u]);
}
