#include "protocol.h"
uint16_t pl_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}
int pl_encode(uint8_t kind, uint16_t node_id, uint16_t seq, uint8_t flags,
              const uint8_t *payload, uint8_t payload_len,
              uint8_t *out, size_t capacity, size_t *written) {
    if (!out || !written || payload_len > PL_MAX_PAYLOAD || (payload_len && !payload)) return -1;
    size_t total = PL_HEADER_SIZE + payload_len + PL_CRC_SIZE;
    if (capacity < total) return -2;
    out[0]=PL_VERSION; out[1]=kind; out[2]=(uint8_t)node_id; out[3]=(uint8_t)(node_id>>8);
    out[4]=(uint8_t)seq; out[5]=(uint8_t)(seq>>8); out[6]=payload_len; out[7]=flags;
    for (uint8_t i=0; i<payload_len; ++i) out[PL_HEADER_SIZE+i]=payload[i];
    uint16_t crc=pl_crc16(out,total-PL_CRC_SIZE);
    out[total-2]=(uint8_t)crc; out[total-1]=(uint8_t)(crc>>8); *written=total; return 0;
}
int pl_decode(const uint8_t *f, size_t length, uint8_t *kind, uint16_t *node_id,
              uint16_t *seq, uint8_t *flags, const uint8_t **payload, uint8_t *payload_len) {
    if (!f || !kind || !node_id || !seq || !flags || !payload || !payload_len) return -1;
    if (length < PL_HEADER_SIZE+PL_CRC_SIZE || length > PL_MAX_FRAME || f[0]!=PL_VERSION) return -2;
    uint8_t n=f[6]; if (n>PL_MAX_PAYLOAD || length != PL_HEADER_SIZE+(size_t)n+PL_CRC_SIZE) return -3;
    uint16_t got=(uint16_t)f[length-2] | (uint16_t)((uint16_t)f[length-1]<<8);
    if (got != pl_crc16(f,length-2)) return -4;
    *kind=f[1]; *node_id=(uint16_t)f[2] | (uint16_t)((uint16_t)f[3]<<8);
    *seq=(uint16_t)f[4] | (uint16_t)((uint16_t)f[5]<<8); *payload_len=n; *flags=f[7]; *payload=f+PL_HEADER_SIZE; return 0;
}
