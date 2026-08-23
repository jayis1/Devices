#ifndef PERIODSYNC_PROTOCOL_H
#define PERIODSYNC_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define PSYNC_VERSION 1
#define PSYNC_MAX_PAYLOAD 48
#define PSYNC_FRAME_SIZE 62

typedef enum {
    PSYNC_MSG_TELEMETRY = 0x01,
    PSYNC_MSG_ALERT = 0x02,
    PSYNC_MSG_COMMAND = 0x03,
    PSYNC_MSG_ACK = 0x04,
    PSYNC_MSG_BULK_SYNC = 0x05
} psync_message_type_t;

typedef struct {
    uint8_t version;
    uint8_t message_type;
    uint16_t source_node_id;
    uint16_t destination_node_id;
    uint32_t unix_time;
    uint8_t flags;
    uint8_t payload_length;
    uint8_t payload[PSYNC_MAX_PAYLOAD];
    uint16_t crc;
} psync_frame_t;

uint16_t crc16_ccitt(const uint8_t *data, uint16_t length);
bool psync_build_frame(psync_frame_t *frame,
                       uint8_t message_type,
                       uint16_t src,
                       uint16_t dst,
                       uint32_t unix_time,
                       uint8_t flags,
                       const uint8_t *payload,
                       uint8_t payload_length);
bool psync_encode_frame(const psync_frame_t *frame, uint8_t *buffer, uint16_t buffer_length);
bool psync_decode_frame(const uint8_t *buffer, uint16_t length, psync_frame_t *frame);

#endif
