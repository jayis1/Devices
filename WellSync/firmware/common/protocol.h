#ifndef WSYNC_PROTOCOL_H
#define WSYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WSYNC_NODE_HUB               0x0001
#define WSYNC_NODE_INLINE_WQ         0x0002
#define WSYNC_NODE_PUMP_CTRL         0x0003
#define WSYNC_NODE_TAP_BASE          0x0100
#define WSYNC_NODE_WEATHER           0x0004
#define WSYNC_NODE_BROADCAST         0xFFFF

#define WSYNC_MSG_JOIN_REQ           0x01
#define WSYNC_MSG_JOIN_ACK           0x02
#define WSYNC_MSG_HEALTH             0x03
#define WSYNC_MSG_WATER_QUALITY      0x04
#define WSYNC_MSG_PUMP_STATE         0x05
#define WSYNC_MSG_TAP_EVENT          0x06
#define WSYNC_MSG_WEATHER            0x07
#define WSYNC_MSG_ALERT              0x08
#define WSYNC_MSG_COMMAND            0x09
#define WSYNC_MSG_OTA_STATUS         0x0A

#define WSYNC_ALERT_SAFE             0
#define WSYNC_ALERT_WATCH            1
#define WSYNC_ALERT_TREAT            2
#define WSYNC_ALERT_DO_NOT_DRINK     3

#define WSYNC_FRAME_PAYLOAD_MAX      48
#define WSYNC_SYNC_0                 0x57
#define WSYNC_SYNC_1                 0xA9

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
    uint8_t payload[WSYNC_FRAME_PAYLOAD_MAX];
    uint16_t crc;
} wsync_frame_t;

typedef struct {
    int16_t ph_x100;
    uint16_t ec_us_cm;
    int16_t orp_mv;
    uint16_t turbidity_ntu_x10;
    uint16_t pressure_kpa;
    int16_t temp_c_x100;
    uint8_t advisory_state;
    uint8_t reserved;
} wsync_water_quality_t;

typedef struct {
    uint16_t rms_current_x100;
    uint16_t peak_start_current_x100;
    uint16_t pressure_rise_kpa;
    uint16_t starts_per_hour_x10;
    uint8_t cavitation_score_pct;
    uint8_t short_cycle_score_pct;
    uint8_t dry_run_score_pct;
    uint8_t enabled;
} wsync_pump_state_t;

typedef struct {
    uint16_t tap_id;
    uint16_t flow_pulses;
    int16_t temp_c_x100;
    uint16_t uv_lux;
    uint8_t filter_days_remaining;
    uint8_t door_open;
    uint8_t advisory_ack;
    uint8_t reserved;
} wsync_tap_event_t;

typedef struct {
    uint16_t rain_mm_x10;
    uint16_t soil_shallow_pct_x100;
    uint16_t soil_mid_pct_x100;
    uint16_t soil_deep_pct_x100;
    int16_t pressure_hpa_x10;
    int16_t temp_c_x100;
    uint8_t freeze_risk_pct;
    uint8_t dry_spell_days;
} wsync_weather_t;
#pragma pack(pop)

uint16_t wsync_crc16_frame(const uint8_t *data, size_t len);
void wsync_build_frame(wsync_frame_t *frame,
                       uint16_t src,
                       uint16_t dst,
                       uint8_t msg_type,
                       uint16_t seq,
                       uint32_t nonce,
                       const uint8_t *payload,
                       uint8_t payload_len);
bool wsync_validate_frame(const wsync_frame_t *frame, uint8_t payload_len);

#endif
