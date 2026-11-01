#include "protocol.h"
uint16_t nn_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}
int nn_encode(uint8_t *out, size_t cap, uint16_t id, uint16_t seq,
              uint8_t kind, const uint8_t *payload, uint8_t n, size_t *written) {
    if (!out || !written || n > NN_MAX_PAYLOAD || (n && !payload) || cap < NN_HEADER_SIZE + n + 2u) return -1;
    out[0]=NN_VERSION; out[1]=(uint8_t)id; out[2]=(uint8_t)(id>>8);
    out[3]=(uint8_t)seq; out[4]=(uint8_t)(seq>>8); out[5]=kind; out[6]=n;
    for (uint8_t i=0; i<n; ++i) out[NN_HEADER_SIZE+i]=payload[i];
    size_t body=NN_HEADER_SIZE+n; uint16_t crc=nn_crc16(out,body);
    out[body]=(uint8_t)crc; out[body+1]=(uint8_t)(crc>>8); *written=body+2u; return 0;
}
int nn_validate(const uint8_t *frame, size_t length) {
    if (!frame || length < NN_HEADER_SIZE+2u || length > NN_MAX_FRAME || frame[0] != NN_VERSION) return -1;
    uint8_t n=frame[6]; if (n > NN_MAX_PAYLOAD || length != NN_HEADER_SIZE+(size_t)n+2u) return -1;
    size_t body=length-2u; uint16_t got=(uint16_t)frame[body] | ((uint16_t)frame[body+1]<<8);
    return nn_crc16(frame,body)==got ? 0 : -1;
}
