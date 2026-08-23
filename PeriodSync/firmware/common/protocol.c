#include "protocol.h"
#include <string.h>

bool psync_build_frame(psync_frame_t *frame,
                       uint8_t message_type,
                       uint16_t src,
                       uint16_t dst,
                       uint32_t unix_time,
                       uint8_t flags,
                       const uint8_t *payload,
                       uint8_t payload_length) {
    if (!frame || payload_length > PSYNC_MAX_PAYLOAD) {
        return false;
    }
    memset(frame, 0, sizeof(*frame));
    frame->version = PSYNC_VERSION;
    frame->message_type = message_type;
    frame->source_node_id = src;
    frame->destination_node_id = dst;
    frame->unix_time = unix_time;
    frame->flags = flags;
    frame->payload_length = payload_length;
    if (payload && payload_length) {
        memcpy(frame->payload, payload, payload_length);
    }
    return true;
}

bool psync_encode_frame(const psync_frame_t *frame, uint8_t *buffer, uint16_t buffer_length) {
    if (!frame || !buffer || buffer_length < PSYNC_FRAME_SIZE) {
        return false;
    }
    memset(buffer, 0, buffer_length);
    buffer[0] = frame->version;
    buffer[1] = frame->message_type;
    buffer[2] = (uint8_t)(frame->source_node_id >> 8);
    buffer[3] = (uint8_t)(frame->source_node_id & 0xFF);
    buffer[4] = (uint8_t)(frame->destination_node_id >> 8);
    buffer[5] = (uint8_t)(frame->destination_node_id & 0xFF);
    buffer[6] = (uint8_t)(frame->unix_time >> 24);
    buffer[7] = (uint8_t)(frame->unix_time >> 16);
    buffer[8] = (uint8_t)(frame->unix_time >> 8);
    buffer[9] = (uint8_t)(frame->unix_time & 0xFF);
    buffer[10] = frame->flags;
    buffer[11] = frame->payload_length;
    memcpy(&buffer[12], frame->payload, frame->payload_length);
    uint16_t crc = crc16_ccitt(buffer, 60);
    buffer[60] = (uint8_t)(crc >> 8);
    buffer[61] = (uint8_t)(crc & 0xFF);
    return true;
}

bool psync_decode_frame(const uint8_t *buffer, uint16_t length, psync_frame_t *frame) {
    if (!buffer || !frame || length < PSYNC_FRAME_SIZE) {
        return false;
    }
    uint16_t rx_crc = ((uint16_t)buffer[60] << 8) | buffer[61];
    if (crc16_ccitt(buffer, 60) != rx_crc) {
        return false;
    }
    memset(frame, 0, sizeof(*frame));
    frame->version = buffer[0];
    frame->message_type = buffer[1];
    frame->source_node_id = ((uint16_t)buffer[2] << 8) | buffer[3];
    frame->destination_node_id = ((uint16_t)buffer[4] << 8) | buffer[5];
    frame->unix_time = ((uint32_t)buffer[6] << 24) |
                       ((uint32_t)buffer[7] << 16) |
                       ((uint32_t)buffer[8] << 8) |
                       (uint32_t)buffer[9];
    frame->flags = buffer[10];
    frame->payload_length = buffer[11];
    if (frame->payload_length > PSYNC_MAX_PAYLOAD) {
        return false;
    }
    memcpy(frame->payload, &buffer[12], frame->payload_length);
    frame->crc = rx_crc;
    return true;
}
