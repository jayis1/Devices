#include "protocol.h"

static void put_u16(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)((value >> 8) & 0xFFU);
    dst[1] = (uint8_t)(value & 0xFFU);
}

static uint16_t get_u16(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
}

uint16_t psync_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

bool psync_encode(psync_frame_t *frame, uint8_t *out, size_t out_capacity, size_t *written) {
    size_t total = (size_t)10U + frame->payload_length;
    if (!frame || !out || !written || frame->payload_length > PSYNC_MAX_PAYLOAD || out_capacity < total) {
        return false;
    }
    out[0] = 0xA5U;
    out[1] = frame->version;
    out[2] = frame->type;
    out[3] = frame->source;
    out[4] = frame->destination;
    out[5] = frame->flags;
    put_u16(&out[6], frame->payload_length);
    for (size_t i = 0; i < frame->payload_length; ++i) {
        out[8 + i] = frame->payload[i];
    }
    frame->crc16 = psync_crc16(out, 8U + frame->payload_length);
    put_u16(&out[8 + frame->payload_length], frame->crc16);
    *written = total;
    return true;
}

bool psync_decode(psync_frame_t *frame, const uint8_t *data, size_t length) {
    uint16_t expected_crc;
    if (!frame || !data || length < 10U || data[0] != 0xA5U) {
        return false;
    }
    frame->version = data[1];
    frame->type = data[2];
    frame->source = data[3];
    frame->destination = data[4];
    frame->flags = data[5];
    frame->payload_length = get_u16(&data[6]);
    if (frame->payload_length > PSYNC_MAX_PAYLOAD || length != (size_t)10U + frame->payload_length) {
        return false;
    }
    for (size_t i = 0; i < frame->payload_length; ++i) {
        frame->payload[i] = data[8 + i];
    }
    frame->crc16 = get_u16(&data[8 + frame->payload_length]);
    expected_crc = psync_crc16(data, 8U + frame->payload_length);
    return expected_crc == frame->crc16;
}
