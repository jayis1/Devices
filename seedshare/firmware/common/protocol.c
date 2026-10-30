#include "protocol.h"
uint16_t seed_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}
int seed_frame_valid(const uint8_t *frame, size_t len) {
    if (frame == NULL || len < SEED_OVERHEAD || frame[0] != 1u) return 0;
    const size_t payload = (size_t)frame[8] | ((size_t)frame[9] << 8);
    if (payload > SEED_MAX_PAYLOAD || len != SEED_OVERHEAD + payload) return 0;
    const uint16_t got = (uint16_t)frame[len - 2] | ((uint16_t)frame[len - 1] << 8);
    return seed_crc16(frame, len - 2) == got;
}
