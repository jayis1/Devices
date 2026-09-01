#include "protocol.h"

#include <string.h>

uint16_t commute_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void commute_prepare_frame(commute_frame_t *frame, uint8_t type, uint8_t source, uint8_t dest) {
    memset(frame, 0, sizeof(*frame));
    frame->preamble = COMMUTESYNC_PREAMBLE;
    frame->version = COMMUTESYNC_VERSION;
    frame->message_type = type;
    frame->source_node = source;
    frame->destination_node = dest;
}

size_t commute_encode(commute_frame_t *frame, uint8_t *out_bytes, size_t out_capacity) {
    const size_t total = 8u + frame->payload_length + 2u;
    if (frame->payload_length > COMMUTESYNC_MAX_PAYLOAD || out_capacity < total) {
        return 0u;
    }
    out_bytes[0] = frame->preamble;
    out_bytes[1] = frame->version;
    out_bytes[2] = frame->message_type;
    out_bytes[3] = frame->source_node;
    out_bytes[4] = frame->destination_node;
    out_bytes[5] = frame->flags;
    out_bytes[6] = (uint8_t)(frame->payload_length >> 8);
    out_bytes[7] = (uint8_t)(frame->payload_length & 0xFFu);
    memcpy(&out_bytes[8], frame->payload, frame->payload_length);
    frame->crc16 = commute_crc16(out_bytes, 8u + frame->payload_length);
    out_bytes[8u + frame->payload_length] = (uint8_t)(frame->crc16 >> 8);
    out_bytes[9u + frame->payload_length] = (uint8_t)(frame->crc16 & 0xFFu);
    return total;
}

bool commute_decode(commute_frame_t *frame, const uint8_t *bytes, size_t length) {
    if (length < 10u || bytes[0] != COMMUTESYNC_PREAMBLE) {
        return false;
    }
    const uint16_t payload_length = (uint16_t)((bytes[6] << 8) | bytes[7]);
    if (payload_length > COMMUTESYNC_MAX_PAYLOAD || length != (size_t)(8u + payload_length + 2u)) {
        return false;
    }
    const uint16_t expected = commute_crc16(bytes, 8u + payload_length);
    const uint16_t got = (uint16_t)((bytes[8u + payload_length] << 8) | bytes[9u + payload_length]);
    if (expected != got) {
        return false;
    }
    frame->preamble = bytes[0];
    frame->version = bytes[1];
    frame->message_type = bytes[2];
    frame->source_node = bytes[3];
    frame->destination_node = bytes[4];
    frame->flags = bytes[5];
    frame->payload_length = payload_length;
    memcpy(frame->payload, &bytes[8], payload_length);
    frame->crc16 = got;
    return true;
}
