#include "protocol.h"

size_t rw_encode(const rw_rack_report_t *in, uint8_t *out, size_t cap) {
    size_t i, n;
    if (!in || !out || in->count == 0 || in->count > RW_MAX_BAYS) return 0;
    n = 6u + in->count;
    if (cap < n) return 0;
    out[0] = RW_VERSION; out[1] = in->count;
    out[2] = (uint8_t)in->seq; out[3] = (uint8_t)(in->seq >> 8);
    out[4] = (uint8_t)(in->seq >> 16); out[5] = (uint8_t)(in->seq >> 24);
    for (i = 0; i < in->count; ++i) {
        if (in->bay[i] > RW_OCCUPIED) return 0;
        out[6u + i] = (uint8_t)in->bay[i];
    }
    return n;
}

bool rw_decode(const uint8_t *buf, size_t len, rw_rack_report_t *out) {
    size_t i;
    if (!buf || !out || len < 7u || buf[0] != RW_VERSION || buf[1] == 0 || buf[1] > RW_MAX_BAYS || len != 6u + buf[1]) return false;
    out->count = buf[1];
    out->seq = (uint32_t)buf[2] | ((uint32_t)buf[3] << 8) | ((uint32_t)buf[4] << 16) | ((uint32_t)buf[5] << 24);
    for (i = 0; i < out->count; ++i) {
        if (buf[6u + i] > RW_OCCUPIED) return false;
        out->bay[i] = (rw_state_t)buf[6u + i];
    }
    return true;
}
