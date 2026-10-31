#include "protocol.h"
uint16_t artpath_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xffffu;
    if (data == NULL && len != 0u) return 0u;
    for (size_t i=0; i<len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned b=0; b<8; ++b)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}
int artpath_frame_valid(const uint8_t *frame, size_t len) {
    if (frame == NULL || len < 10u || len > ARTPATH_MAX_FRAME || frame[0] != ARTPATH_VERSION) return 0;
    /* Wire format: version,type,seq(4),event-id(16),payload-len(2),payload,CRC(2). */
    if (len < 26u) return 0;
    size_t payload = ((size_t)frame[22] << 8) | frame[23];
    if (payload > ARTPATH_MAX_FRAME - 26u || len != 26u + payload) return 0;
    uint16_t expected = ((uint16_t)frame[len-2] << 8) | frame[len-1];
    return artpath_crc16(frame, len-2u) == expected;
}
