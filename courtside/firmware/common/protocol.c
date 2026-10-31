#include "protocol.h"

#define FRAME_LEN 15u

size_t courtside_encode(const courtside_sample_t *s, uint8_t *out, size_t cap) {
    if (!s || !out || cap < FRAME_LEN || s->version != COURTSIDE_PROTOCOL_VERSION || s->kind < 1u || s->kind > 3u) return 0;
    out[0]=s->version; out[1]=(uint8_t)(s->sequence>>24); out[2]=(uint8_t)(s->sequence>>16); out[3]=(uint8_t)(s->sequence>>8); out[4]=(uint8_t)s->sequence; out[5]=s->kind;
    out[6]=(uint8_t)((uint32_t)s->wetness_raw>>24); out[7]=(uint8_t)((uint32_t)s->wetness_raw>>16); out[8]=(uint8_t)((uint32_t)s->wetness_raw>>8); out[9]=(uint8_t)s->wetness_raw;
    out[10]=(uint8_t)((uint16_t)s->temperature_centi_c>>8); out[11]=(uint8_t)s->temperature_centi_c;
    out[12]=(uint8_t)(s->humidity_centi_pct>>8); out[13]=(uint8_t)s->humidity_centi_pct;
    out[14]=0; /* Reserved: must be zero in v1. */
    return FRAME_LEN;
}

int courtside_decode(const uint8_t *d, size_t n, courtside_sample_t *s) {
    if (!d || !s || n != FRAME_LEN || d[0] != COURTSIDE_PROTOCOL_VERSION || d[5] < 1u || d[5] > 3u || d[14] != 0u) return -1;
    s->version=d[0]; s->sequence=((uint32_t)d[1]<<24)|((uint32_t)d[2]<<16)|((uint32_t)d[3]<<8)|d[4]; s->kind=d[5];
    s->wetness_raw=(int32_t)(((uint32_t)d[6]<<24)|((uint32_t)d[7]<<16)|((uint32_t)d[8]<<8)|d[9]);
    s->temperature_centi_c=(int16_t)(((uint16_t)d[10]<<8)|d[11]); s->humidity_centi_pct=(uint16_t)(((uint16_t)d[12]<<8)|d[13]);
    return 0;
}
