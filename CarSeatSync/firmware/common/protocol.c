#include "protocol.h"
#include <string.h>

uint16_t csync_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

bool csync_encode(csync_frame_t *frame, uint8_t source_id, csync_message_type_t type, uint32_t trip_id, const uint8_t *payload, uint8_t payload_len) {
    if (frame == NULL || (payload == NULL && payload_len > 0U) || payload_len > CSYNC_MAX_PAYLOAD) {
        return false;
    }
    frame->sync = CSYNC_SYNC_WORD;
    frame->version = CSYNC_PROTO_VERSION;
    frame->source_id = source_id;
    frame->msg_type = (uint8_t)type;
    frame->trip_id = trip_id;
    frame->payload_len = payload_len;
    if (payload_len > 0U) {
        memcpy(frame->payload, payload, payload_len);
    }
    frame->crc16 = csync_crc16((const uint8_t *)frame, offsetof(csync_frame_t, crc16));
    return true;
}

bool csync_validate(const csync_frame_t *frame) {
    if (frame == NULL) {
        return false;
    }
    if (frame->sync != CSYNC_SYNC_WORD || frame->version != CSYNC_PROTO_VERSION || frame->payload_len > CSYNC_MAX_PAYLOAD) {
        return false;
    }
    uint16_t expected = csync_crc16((const uint8_t *)frame, offsetof(csync_frame_t, crc16));
    return expected == frame->crc16;
}
