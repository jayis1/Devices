#ifndef FOOD_ALLERGY_SYNC_PROTOCOL_H
#define FOOD_ALLERGY_SYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define FAS_MAX_PAYLOAD 48
#define FAS_PREAMBLE 0xA5
#define FAS_VERSION 0x01

typedef enum {
    FAS_MSG_TELEMETRY = 0x01,
    FAS_MSG_EVENT = 0x02,
    FAS_MSG_ALERT = 0x03,
    FAS_MSG_COMMAND = 0x04,
    FAS_MSG_ACK = 0x05,
    FAS_MSG_CONFIG = 0x06,
    FAS_MSG_OTA_CHUNK = 0x07
} fas_message_type_t;

typedef enum {
    FAS_NODE_HUB = 0x10,
    FAS_NODE_MEAL_SCANNER = 0x11,
    FAS_NODE_STRIP_READER = 0x12,
    FAS_NODE_SAFE_LUNCH = 0x13,
    FAS_NODE_EPIPEN_GUARD = 0x14
} fas_node_type_t;

typedef struct {
    uint8_t preamble;
    uint8_t version;
    uint8_t message_type;
    uint8_t node_type;
    uint16_t source_id;
    uint16_t destination_id;
    uint8_t flags;
    uint8_t sequence;
    uint8_t payload_length;
    uint8_t payload[FAS_MAX_PAYLOAD];
    uint16_t crc16;
} fas_frame_t;

uint16_t fas_crc16_ccitt(const uint8_t *data, size_t length);
int fas_encode_frame(fas_frame_t *frame, uint8_t *buffer, size_t buffer_size);
int fas_decode_frame(fas_frame_t *frame, const uint8_t *buffer, size_t length);

#endif
