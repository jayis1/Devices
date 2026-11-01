#include <stddef.h>
#include <stdint.h>

#define WAYMARK_VERSION 1u
#define WAYMARK_MAX_PAYLOAD 48u
#define WAYMARK_HEADER_SIZE 8u

/* Wire frame: 'W','M',version,type,node_id_le16,sequence_le16,payload,crc16_le.
 * CRC-16/CCITT-FALSE covers header and payload. */
uint16_t waymark_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

size_t waymark_encode(uint8_t type, uint16_t node, uint16_t seq,
                      const uint8_t *payload, size_t payload_len,
                      uint8_t *out, size_t capacity) {
    if (!out || payload_len > WAYMARK_MAX_PAYLOAD || (payload_len && !payload) ||
        capacity < WAYMARK_HEADER_SIZE + payload_len + 2u) return 0;
    out[0]='W'; out[1]='M'; out[2]=WAYMARK_VERSION; out[3]=type;
    out[4]=(uint8_t)node; out[5]=(uint8_t)(node>>8);
    out[6]=(uint8_t)seq; out[7]=(uint8_t)(seq>>8);
    for (size_t i=0;i<payload_len;i++) out[8+i]=payload[i];
    uint16_t c=waymark_crc16(out,8+payload_len);
    out[8+payload_len]=(uint8_t)c; out[9+payload_len]=(uint8_t)(c>>8);
    return 10+payload_len;
}

int waymark_validate(const uint8_t *frame, size_t len) {
    if (!frame || len < 10 || len > 10+WAYMARK_MAX_PAYLOAD ||
        frame[0]!='W' || frame[1]!='M' || frame[2]!=WAYMARK_VERSION) return 0;
    size_t body=len-2;
    uint16_t got=(uint16_t)frame[body] | (uint16_t)frame[body+1]<<8;
    return waymark_crc16(frame,body)==got;
}
