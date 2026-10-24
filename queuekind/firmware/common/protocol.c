#include "protocol.h"

uint16_t qk_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

size_t qk_encode(uint8_t type, uint8_t node, uint16_t seq, const uint8_t *payload,
                 uint8_t payload_len, uint8_t *out, size_t cap) {
    if (!out || payload_len > QK_MAX_PAYLOAD || (payload_len && !payload) || cap < (size_t)(9u + payload_len)) return 0;
    out[0]=0x51; out[1]=0x4b; out[2]=QK_VERSION; out[3]=type; out[4]=node;
    out[5]=(uint8_t)seq; out[6]=(uint8_t)(seq >> 8); out[7]=payload_len;
    for (uint8_t i=0;i<payload_len;i++) out[8+i]=payload[i];
    uint16_t crc=qk_crc16(out,8u+payload_len);
    out[8+payload_len]=(uint8_t)crc; out[9+payload_len]=(uint8_t)(crc>>8);
    return 10u+payload_len;
}

int qk_decode(const uint8_t *in, size_t len, qk_frame_t *f) {
    if (!in || !f || len < 10u || len > QK_MAX_FRAME || in[0]!=0x51 || in[1]!=0x4b || in[2]!=QK_VERSION) return 0;
    uint8_t n=in[7];
    if (n>QK_MAX_PAYLOAD || len != 10u+n || in[3]<QK_ARRIVAL || in[3]>QK_RESUME) return 0;
    uint16_t got=(uint16_t)in[len-2] | ((uint16_t)in[len-1]<<8);
    if (got != qk_crc16(in,len-2)) return 0;
    f->type=in[3]; f->node=in[4]; f->seq=(uint16_t)in[5] | ((uint16_t)in[6]<<8); f->payload_len=n;
    for (uint8_t i=0;i<n;i++) f->payload[i]=in[8+i];
    return 1;
}
