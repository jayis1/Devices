#include "protocol.h"

#include <string.h>

uint16_t ms_crc16_ccitt(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

int ms_encode_frame(ms_frame_t *frame, uint8_t *buffer, size_t buffer_size) {
    if (!frame || !buffer || frame->payload_length > MS_MAX_PAYLOAD) {
        return -1;
    }

    size_t length = (size_t)(11 + frame->payload_length + 2);
    if (buffer_size < length) {
        return -2;
    }

    buffer[0] = frame->preamble;
    buffer[1] = frame->version;
    buffer[2] = frame->message_type;
    buffer[3] = frame->node_type;
    buffer[4] = (uint8_t)(frame->source_id >> 8);
    buffer[5] = (uint8_t)(frame->source_id & 0xFFu);
    buffer[6] = (uint8_t)(frame->destination_id >> 8);
    buffer[7] = (uint8_t)(frame->destination_id & 0xFFu);
    buffer[8] = frame->flags;
    buffer[9] = frame->sequence;
    buffer[10] = frame->payload_length;
    memcpy(&buffer[11], frame->payload, frame->payload_length);
    frame->crc16 = ms_crc16_ccitt(buffer, 11 + frame->payload_length);
    buffer[11 + frame->payload_length] = (uint8_t)(frame->crc16 >> 8);
    buffer[12 + frame->payload_length] = (uint8_t)(frame->crc16 & 0xFFu);
    return (int)length;
}

int ms_decode_frame(ms_frame_t *frame, const uint8_t *buffer, size_t length) {
    if (!frame || !buffer || length < 13) {
        return -1;
    }
    if (buffer[0] != MS_PREAMBLE || buffer[1] != MS_VERSION) {
        return -2;
    }

    uint8_t payload_length = buffer[10];
    if (payload_length > MS_MAX_PAYLOAD || length != (size_t)(11 + payload_length + 2)) {
        return -3;
    }

    uint16_t expected = ms_crc16_ccitt(buffer, 11 + payload_length);
    uint16_t actual = (uint16_t)(((uint16_t)buffer[11 + payload_length] << 8) | buffer[12 + payload_length]);
    if (expected != actual) {
        return -4;
    }

    frame->preamble = buffer[0];
    frame->version = buffer[1];
    frame->message_type = buffer[2];
    frame->node_type = buffer[3];
    frame->source_id = (uint16_t)(((uint16_t)buffer[4] << 8) | buffer[5]);
    frame->destination_id = (uint16_t)(((uint16_t)buffer[6] << 8) | buffer[7]);
    frame->flags = buffer[8];
    frame->sequence = buffer[9];
    frame->payload_length = payload_length;
    memcpy(frame->payload, &buffer[11], payload_length);
    frame->crc16 = actual;
    return 0;
}
