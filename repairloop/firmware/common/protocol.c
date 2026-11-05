#include "protocol.h"
#include <string.h>
int rl_encode(const rl_message *m, uint8_t *out, size_t cap, size_t *written) {
    size_t n;
    if (!m || !out || !written || m->length > RL_MAX_PAYLOAD) return -1;
    n = 7u + m->length;
    if (cap < n) return -2;
    out[0] = RL_VERSION; out[1] = m->type;
    out[2] = m->length; out[3] = (uint8_t)(m->sequence >> 24);
    out[4] = (uint8_t)(m->sequence >> 16); out[5] = (uint8_t)(m->sequence >> 8);
    out[6] = (uint8_t)m->sequence;
    memcpy(out + 7, m->payload, m->length); *written = n; return 0;
}
int rl_decode(const uint8_t *data, size_t n, rl_message *out) {
    size_t i;
    if (!data || !out || n < 7u || data[0] != RL_VERSION || data[2] > RL_MAX_PAYLOAD || n != 7u + data[2]) return -1;
    out->type = data[1]; out->length = data[2];
    out->sequence = ((uint32_t)data[3] << 24) | ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 8) | data[6];
    for (i = 0; i < out->length; ++i) out->payload[i] = data[7 + i];
    return 0;
}
