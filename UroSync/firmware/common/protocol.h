#ifndef UROSYNC_PROTOCOL_H
#define UROSYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define US_NODE_HUB               0x0001
#define US_NODE_TOILET_DOCK       0x0002
#define US_NODE_NIGHT_MAT         0x0003
#define US_NODE_BOTTLE_TAG        0x0004
#define US_NODE_BATH_SENTINEL     0x0005
#define US_NODE_BROADCAST         0xFFFF

#define US_MSG_JOIN_REQ           0x01
#define US_MSG_JOIN_ACK           0x02
#define US_MSG_HEARTBEAT          0x03
#define US_MSG_VOID_EVENT         0x04
#define US_MSG_MAT_EVENT          0x05
#define US_MSG_BOTTLE_EVENT       0x06
#define US_MSG_ENV_EVENT          0x07
#define US_MSG_COMMAND            0x08
#define US_MSG_CONFIG             0x09
#define US_MSG_ALERT              0x0A
#define US_MSG_OTA_CHUNK          0x0B
#define US_MSG_OTA_STATUS         0x0C

#define US_ALERT_NONE             0
#define US_ALERT_HYDRATION        1
#define US_ALERT_UTI              2
#define US_ALERT_FALL_RISK        3
#define US_ALERT_LEAK             4

#define US_FRAME_PAYLOAD_MAX      48
#define US_SYNC_0                 0x52
#define US_SYNC_1                 0xA7

#pragma pack(push, 1)
typedef struct {
    uint8_t preamble[4];
    uint8_t sync[2];
    uint8_t length;
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t msg_type;
    uint16_t seq;
    uint32_t session_nonce;
    uint8_t payload[US_FRAME_PAYLOAD_MAX];
    uint16_t crc;
} us_frame_t;

typedef struct {
    uint32_t event_id;
    uint16_t volume_ml;
    uint16_t peak_flow_ml_min;
    uint16_t flow_duration_s;
    uint8_t color_index;
    uint8_t leukocyte;
    uint8_t nitrite;
    uint8_t blood;
    uint8_t protein;
    uint8_t ketone;
    uint8_t glucose;
    uint8_t ph_bin;
    uint16_t sg_q1000;
} us_void_event_t;

typedef struct {
    uint16_t transfer_latency_ms;
    uint16_t sway_index;
    uint16_t left_load_pct;
    uint16_t right_load_pct;
    int16_t left_temp_c_x100;
    int16_t right_temp_c_x100;
    uint16_t trip_duration_s;
    uint8_t slip_flag;
} us_mat_event_t;

typedef struct {
    uint16_t bottle_mass_g;
    uint16_t consumed_ml_day;
    uint16_t sip_count_day;
    uint8_t adherence_score;
    uint8_t reminder_acked;
} us_bottle_event_t;

typedef struct {
    int16_t temp_c_x100;
    uint16_t rh_pct_x100;
    uint16_t voc_index;
    uint8_t leak_detected;
    uint8_t fan_active;
    uint16_t lux;
} us_env_event_t;
#pragma pack(pop)

uint16_t us_crc16(const uint8_t *data, size_t len);
void us_build_frame(us_frame_t *frame,
                    uint16_t src,
                    uint16_t dst,
                    uint8_t msg_type,
                    uint16_t seq,
                    uint32_t nonce,
                    const uint8_t *payload,
                    uint8_t payload_len);
bool us_validate_frame(const us_frame_t *frame, uint8_t payload_len);

#endif
