#include "protocol.h"
#include <stddef.h>

uint16_t cupcycle_crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= (uint16_t)p[i] << 8;
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

int cupcycle_encode(const cupcycle_frame_t *f, uint8_t out[CUPCYCLE_FRAME_LEN]) {
    if (!f || !out || f->version != 1 || f->node < 1 || f->node > 3 || f->kind < 1 || f->kind > 3) return -1;
    out[0]=f->version; out[1]=f->node; out[2]=f->kind;
    out[3]=(uint8_t)f->sequence; out[4]=(uint8_t)(f->sequence >> 8);
    for (unsigned i=0;i<4;i++) out[5+i]=(uint8_t)(f->unix_s >> (8*i));
    out[9]=(uint8_t)f->value; out[10]=(uint8_t)(f->value >> 8);
    uint16_t c=cupcycle_crc16(out,11); out[11]=(uint8_t)c; out[12]=(uint8_t)(c>>8);
    return 0;
}

int cupcycle_decode(const uint8_t *in, size_t n, cupcycle_frame_t *f) {
    if (!in || !f || n != CUPCYCLE_FRAME_LEN || in[0] != 1 || in[1] < 1 || in[1] > 3 || in[2] < 1 || in[2] > 3) return -1;
    uint16_t expected=(uint16_t)in[11] | ((uint16_t)in[12] << 8);
    if (cupcycle_crc16(in,11) != expected) return -2;
    f->version=in[0]; f->node=in[1]; f->kind=in[2];
    f->sequence=(uint16_t)in[3] | ((uint16_t)in[4]<<8);
    f->unix_s=(uint32_t)in[5] | ((uint32_t)in[6]<<8) | ((uint32_t)in[7]<<16) | ((uint32_t)in[8]<<24);
    f->value=(uint16_t)in[9] | ((uint16_t)in[10]<<8);
    return 0;
}
