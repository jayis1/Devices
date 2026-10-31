#include "protocol.h"

uint16_t cr_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
    }
    return crc;
}

int cr_encode(const cr_message_t *m, uint8_t *out, uint16_t capacity) {
    if (!m || !out || m->version != CR_VERSION || m->payload_len > CR_MAX_PAYLOAD) return -1;
    uint16_t n = CR_HEADER_LEN + m->payload_len;
    if (capacity < n + 2u) return -2;
    out[0]=m->version; out[1]=m->node_id; out[2]=m->type; out[3]=m->seq;
    for (uint8_t i=0;i<4;i++) out[4+i]=(uint8_t)(m->timestamp_s >> (8u*i));
    for (uint8_t i=0;i<m->payload_len;i++) out[CR_HEADER_LEN+i]=m->payload[i];
    uint16_t crc=cr_crc16(out,n); out[n]=(uint8_t)crc; out[n+1]=(uint8_t)(crc>>8);
    return (int)(n+2u);
}

int cr_decode(const uint8_t *buf, uint16_t length, cr_message_t *m) {
    if (!buf || !m || length < CR_HEADER_LEN+2u || length > CR_FRAME_MAX) return -1;
    uint8_t payload_len=(uint8_t)(length-CR_HEADER_LEN-2u);
    if (payload_len > CR_MAX_PAYLOAD || buf[0] != CR_VERSION) return -2;
    uint16_t n=(uint16_t)(length-2u), got=(uint16_t)(buf[n] | ((uint16_t)buf[n+1]<<8));
    if (cr_crc16(buf,n)!=got) return -3;
    m->version=buf[0]; m->node_id=buf[1]; m->type=buf[2]; m->seq=buf[3];
    m->timestamp_s=(uint32_t)buf[4]|((uint32_t)buf[5]<<8)|((uint32_t)buf[6]<<16)|((uint32_t)buf[7]<<24);
    m->payload_len=payload_len;
    for (uint8_t i=0;i<payload_len;i++) m->payload[i]=buf[CR_HEADER_LEN+i];
    return 0;
}
