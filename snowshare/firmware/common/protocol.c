#include "protocol.h"

static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t c = 0xffffu;
    for (size_t i = 0; i < n; ++i) {
        c ^= (uint16_t)p[i] << 8;
        for (unsigned b = 0; b < 8; ++b) c = (c & 0x8000u) ? (uint16_t)((c << 1) ^ 0x1021u) : (uint16_t)(c << 1);
    }
    return c;
}
int ss_encode(uint8_t type, const uint8_t *payload, uint16_t n, uint8_t *out, size_t cap, size_t *written) {
    if (!out || !written || (n && !payload) || n > SS_MAX_PAYLOAD || cap < (size_t)n + 5u) return -1;
    out[0] = 1; out[1] = type; out[2] = (uint8_t)n; out[3] = (uint8_t)(n >> 8);
    for (uint16_t i = 0; i < n; ++i) out[4u+i] = payload[i];
    uint16_t c = crc16(out, (size_t)n + 4u);
    out[4u+n] = (uint8_t)c; out[5u+n] = (uint8_t)(c >> 8);
    *written = (size_t)n + 6u;
    return 0;
}
int ss_decode(const uint8_t *f, size_t n, uint8_t *type, uint8_t *p, size_t cap, uint16_t *plen) {
    if (!f || !type || !plen || n < 6 || f[0] != 1) return -1;
    uint16_t l = (uint16_t)f[2] | ((uint16_t)f[3] << 8);
    if (l > SS_MAX_PAYLOAD || n != (size_t)l + 6u || cap < l || (l && !p)) return -1;
    uint16_t got = (uint16_t)f[4u+l] | ((uint16_t)f[5u+l] << 8);
    if (got != crc16(f, (size_t)l + 4u)) return -1;
    for (uint16_t i = 0; i < l; ++i) p[i] = f[4u+i];
    *type=f[1]; *plen=l; return 0;
}
