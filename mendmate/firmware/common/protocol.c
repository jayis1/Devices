#include "protocol.h"

static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= (uint16_t)p[i] << 8;
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}
size_t mm_encode(const mm_packet_t *p, uint8_t *out, size_t cap) {
    if (!p || !out || p->length > MM_MAX_PAYLOAD || cap < (size_t)(10u + p->length)) return 0;
    out[0] = MM_VERSION; out[1] = p->node; out[2] = p->kind; out[3] = p->length;
    for (unsigned i = 0; i < 4; ++i) out[4+i] = (uint8_t)(p->seq >> (8u*i));
    for (unsigned i = 0; i < p->length; ++i) out[8+i] = p->payload[i];
    uint16_t c = crc16(out, 8u + p->length);
    out[8u+p->length] = (uint8_t)c; out[9u+p->length] = (uint8_t)(c >> 8);
    return 10u + p->length;
}
bool mm_decode(const uint8_t *b, size_t n, mm_packet_t *p) {
    if (!b || !p || n < 10u || b[0] != MM_VERSION || b[3] > MM_MAX_PAYLOAD || n != 10u + b[3]) return false;
    uint16_t got = (uint16_t)b[n-2] | (uint16_t)b[n-1] << 8;
    if (got != crc16(b, n-2)) return false;
    p->node=b[1]; p->kind=b[2]; p->length=b[3]; p->seq=0;
    for (unsigned i=0;i<4;++i) p->seq |= (uint32_t)b[4+i] << (8u*i);
    for (unsigned i=0;i<p->length;++i) p->payload[i]=b[8+i];
    return true;
}
