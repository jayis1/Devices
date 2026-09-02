#ifndef SICKDAYSYNC_PROTOCOL_H
#define SICKDAYSYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SDS_MAX_PAYLOAD 56U

typedef enum {
    SDS_NODE_HUB = 1,
    SDS_NODE_RECOVERY_BAND = 2,
    SDS_NODE_ROOM_SENTINEL = 3,
    SDS_NODE_MED_STATION = 4,
    SDS_NODE_VENT_CONTROLLER = 5
} sds_node_type_t;

typedef enum {
    SDS_KIND_TELEMETRY = 1,
    SDS_KIND_COMMAND = 2,
    SDS_KIND_ALERT = 3,
    SDS_KIND_ACK = 4
} sds_frame_kind_t;

typedef struct {
    uint16_t src;
    uint16_t dst;
    uint8_t room_id;
    uint8_t kind;
    uint32_t epoch_s;
    uint8_t payload_len;
    uint8_t payload[SDS_MAX_PAYLOAD];
    uint16_t crc;
} sds_frame_t;

uint16_t sds_crc16_ccitt(const uint8_t *data, size_t len);
size_t sds_encode_frame(const sds_frame_t *frame, uint8_t *out, size_t out_len);
bool sds_decode_frame(const uint8_t *encoded, size_t encoded_len, sds_frame_t *out_frame);
float sds_clampf(float value, float low, float high);

#endif
