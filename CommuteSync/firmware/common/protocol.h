#ifndef COMMUTESYNC_PROTOCOL_H
#define COMMUTESYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COMMUTESYNC_MAX_PAYLOAD 128u
#define COMMUTESYNC_PREAMBLE 0xA5u
#define COMMUTESYNC_VERSION 1u

enum commute_message_type {
    MSG_HEARTBEAT = 0x01,
    MSG_READINESS = 0x10,
    MSG_BAG_TAMPER = 0x11,
    MSG_ROUTE_SAMPLE = 0x12,
    MSG_ARRIVAL = 0x13,
    MSG_MODEL_SCORE = 0x20,
    MSG_INTERVENTION = 0x30,
    MSG_OTA_FRAGMENT = 0x40,
    MSG_FAULT = 0x7F
};

typedef struct {
    uint8_t preamble;
    uint8_t version;
    uint8_t message_type;
    uint8_t source_node;
    uint8_t destination_node;
    uint8_t flags;
    uint16_t payload_length;
    uint8_t payload[COMMUTESYNC_MAX_PAYLOAD];
    uint16_t crc16;
} commute_frame_t;

typedef struct {
    uint8_t required_items;
    uint8_t confirmed_items;
    uint8_t missing_flags;
    uint8_t departure_in_minutes;
} readiness_payload_t;

typedef struct {
    uint8_t tamper_score_x100;
    uint8_t motion_state;
    uint16_t separation_cm;
    uint16_t battery_mv;
} bag_payload_t;

typedef struct {
    uint16_t route_minutes;
    int16_t eta_delta_minutes;
    uint16_t pm25_x10;
    uint16_t voc_index;
    uint16_t vibration_rms_x100;
    uint8_t crash_flag;
} route_payload_t;

typedef struct {
    uint8_t arrival_confirmed;
    uint8_t items_left_behind;
    uint8_t laptop_present;
    uint8_t bag_present;
} arrival_payload_t;

uint16_t commute_crc16(const uint8_t *data, size_t length);
size_t commute_encode(commute_frame_t *frame, uint8_t *out_bytes, size_t out_capacity);
bool commute_decode(commute_frame_t *frame, const uint8_t *bytes, size_t length);
void commute_prepare_frame(commute_frame_t *frame, uint8_t type, uint8_t source, uint8_t dest);

#endif
