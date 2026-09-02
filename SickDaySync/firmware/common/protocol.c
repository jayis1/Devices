#include "protocol.h"

static void write_u16(uint8_t *buf, uint16_t value) {
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFFu);
}

static uint16_t read_u16(const uint8_t *buf) {
    return (uint16_t)((buf[0] << 8) | buf[1]);
}

static void write_u32(uint8_t *buf, uint32_t value) {
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)(value & 0xFFu);
}

static uint32_t read_u32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

uint16_t sds_crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

size_t sds_encode_frame(const sds_frame_t *frame, uint8_t *out, size_t out_len) {
    if (!frame || !out || frame->payload_len > SDS_MAX_PAYLOAD) {
        return 0U;
    }
    const size_t needed = 12U + frame->payload_len;
    if (out_len < needed) {
        return 0U;
    }
    write_u16(&out[0], frame->src);
    write_u16(&out[2], frame->dst);
    out[4] = frame->room_id;
    out[5] = frame->kind;
    write_u32(&out[6], frame->epoch_s);
    out[10] = frame->payload_len;
    for (uint8_t i = 0; i < frame->payload_len; ++i) {
        out[11 + i] = frame->payload[i];
    }
    const uint16_t crc = sds_crc16_ccitt(out, 11U + frame->payload_len);
    write_u16(&out[11 + frame->payload_len], crc);
    return needed + 1U;
}

bool sds_decode_frame(const uint8_t *encoded, size_t encoded_len, sds_frame_t *out_frame) {
    if (!encoded || !out_frame || encoded_len < 13U) {
        return false;
    }
    const uint8_t payload_len = encoded[10];
    if (payload_len > SDS_MAX_PAYLOAD || encoded_len != (size_t)(13U + payload_len)) {
        return false;
    }
    const uint16_t expected = read_u16(&encoded[11 + payload_len]);
    if (sds_crc16_ccitt(encoded, 11U + payload_len) != expected) {
        return false;
    }
    out_frame->src = read_u16(&encoded[0]);
    out_frame->dst = read_u16(&encoded[2]);
    out_frame->room_id = encoded[4];
    out_frame->kind = encoded[5];
    out_frame->epoch_s = read_u32(&encoded[6]);
    out_frame->payload_len = payload_len;
    for (uint8_t i = 0; i < payload_len; ++i) {
        out_frame->payload[i] = encoded[11 + i];
    }
    out_frame->crc = expected;
    return true;
}

float sds_clampf(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}
