#ifndef PREGNANCYSYNC_PROTOCOL_H
#define PREGNANCYSYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PSYNC_MAX_PAYLOAD 96U

typedef enum {
    PSYNC_MSG_HEARTBEAT = 0x01,
    PSYNC_MSG_BAND_SUMMARY = 0x10,
    PSYNC_MSG_CUFF_SUMMARY = 0x11,
    PSYNC_MSG_STRIP_RESULT = 0x12,
    PSYNC_MSG_PAD_SUMMARY = 0x13,
    PSYNC_MSG_RISK_UPDATE = 0x20,
    PSYNC_MSG_ACTION = 0x30,
    PSYNC_MSG_FAULT = 0x7F
} psync_message_type_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint8_t source;
    uint8_t destination;
    uint8_t flags;
    uint16_t payload_length;
    uint8_t payload[PSYNC_MAX_PAYLOAD];
    uint16_t crc16;
} psync_frame_t;

uint16_t psync_crc16(const uint8_t *data, size_t length);
bool psync_encode(psync_frame_t *frame, uint8_t *out, size_t out_capacity, size_t *written);
bool psync_decode(psync_frame_t *frame, const uint8_t *data, size_t length);

#endif
