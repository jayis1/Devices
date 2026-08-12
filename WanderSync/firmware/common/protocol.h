/*
 * WanderSync — Protocol Header
 * Binary message encoding/decoding for Sub-GHz TDMA mesh
 */
#ifndef WANDERSYNC_PROTOCOL_H
#define WANDERSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define WS_SYNC0             0x57  /* 'W' */
#define WS_SYNC1             0x53  /* 'S' */
#define WS_BROADCAST         0xFF
#define WS_MAX_PAYLOAD       240
#define WS_MAX_MSG           256
#define WS_HEADER_LEN        7      /* sync(2)+src(1)+dst(1)+type(1)+msgid(2) */
#define WS_CRC_LEN           2      /* CRC-16-CCITT */

/* Message types */
enum ws_msg_type {
    WS_MSG_JOIN_REQ      = 0x01,
    WS_MSG_JOIN_ACK      = 0x02,
    WS_MSG_TELEMETRY     = 0x03,
    WS_MSG_COMMAND       = 0x04,
    WS_MSG_CMD_ACK       = 0x05,
    WS_MSG_WANDER_ALERT  = 0x06,   /* EMERGENCY — wander/fall/SOS/band_removed */
    WS_MSG_FALL_ALERT    = 0x07,   /* EMERGENCY */
    WS_MSG_SOS_ALERT     = 0x08,   /* EMERGENCY */
    WS_MSG_DOOR_ALERT    = 0x09,   /* High — door state change / tamper */
    WS_MSG_DOOR_LOCK_CMD = 0x0A,   /* High — lock/unlock command */
    WS_MSG_DOOR_LOCK_ACK = 0x0B,   /* High — lock result */
    WS_MSG_REMINDER_TRIG = 0x0C,   /* Normal — trigger voice reminder */
    WS_MSG_REMINDER_ACK  = 0x0D,   /* Normal — reminder played */
    WS_MSG_ADL_UPDATE    = 0x0E,   /* Normal — room activity update */
    WS_MSG_ANOMALY_ALERT = 0x0F,   /* High — behavioral anomaly */
    WS_MSG_EMERGENCY_DISP= 0x10,   /* EMERGENCY — 911 dispatch via 4G LTE */
    WS_MSG_OTA_BLOCK     = 0x11,
    WS_MSG_OTA_ACK       = 0x12,
    WS_MSG_HEARTBEAT     = 0x13,
    WS_MSG_GEOFENCE_UPD  = 0x14,   /* Hub→Band geofence config */
    WS_MSG_GEOFENCE_ACK  = 0x15,
    WS_MSG_TIME_SYNC     = 0x16,
    WS_MSG_SILENCE       = 0x17,
    WS_MSG_TEST_ALARM    = 0x18,
    WS_MSG_CALIBRATION   = 0x19,
    WS_MSG_CALIB_ACK     = 0x1A,
    WS_MSG_TAMPER_ALERT  = 0x1B,
    WS_MSG_BAND_REMOVED  = 0x1C,   /* High — band taken off wrist */
    WS_MSG_REMINDER_SCHED= 0x1D,   /* Hub→Voice full schedule (24 slots) */
};

/* Telemetry sub-types */
enum ws_telem_subtype {
    WS_TELEM_BAND = 0x01,
    WS_TELEM_DOOR = 0x02,
    WS_TELEM_ROOM = 0x03,
    WS_TELEM_VOICE= 0x04,
    WS_TELEM_HUB  = 0x05,
};

/* Alert types (in Wander Alert payload) */
enum ws_alert_type {
    WS_ALERT_TYPE_WANDER  = 1,
    WS_ALERT_TYPE_FALL    = 2,
    WS_ALERT_TYPE_SOS     = 3,
    WS_ALERT_TYPE_BAND_OFF= 4,
};

/* Command sub-types */
enum ws_cmd_type {
    WS_CMD_LOCK_DOOR       = 0x01,
    WS_CMD_UNLOCK_DOOR     = 0x02,
    WS_CMD_EMERGENCY_UNLOCK= 0x03,
    WS_CMD_TRIGGER_REMINDER= 0x04,
    WS_CMD_SILENCE         = 0x05,
    WS_CMD_TEST_ALARM      = 0x06,
    WS_CMD_SET_GEOFENCE    = 0x07,
    WS_CMD_SET_SCHEDULE    = 0x08,
    WS_CMD_OTA_START       = 0x09,
    WS_CMD_REBOOT          = 0x0A,
    WS_CMD_PLAY_CLIP       = 0x0B,
};

/* Message header */
typedef struct {
    uint8_t  src;
    uint8_t  dst;
    uint8_t  type;
    uint16_t msg_id;
} ws_header_t;

/* Full message */
typedef struct {
    ws_header_t header;
    uint8_t  payload[WS_MAX_PAYLOAD];
    uint8_t  payload_len;
} ws_message_t;

/* === Wander/Fall/SOS Alert payload (16 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  alert_type;       /* 1=wander, 2=fall, 3=sos, 4=band_removed */
    int32_t  gps_lat_e7;       /* Latitude × 1e7 (signed) */
    int32_t  gps_lon_e7;       /* Longitude × 1e7 (signed) */
    uint8_t  wander_risk;      /* WanderNet risk score (0-100) */
    uint8_t  activity_class;   /* Current activity */
    uint8_t  geofence_status;  /* 0=inside, 1=outside, 2=near */
    uint8_t  battery_v;       /* ×0.01V */
    uint8_t  impact_g_x10;     /* Fall impact (×0.1g, 0 if not fall) */
    uint8_t  band_on_wrist;    /* 0=removed, 1=worn */
    uint8_t  hr_bpm;           /* Heart rate at alert time */
} ws_wander_alert_t;

/* === Door Lock Command payload (6 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  door_id;       /* 0xFF = all doors */
    uint8_t  action;        /* 0=unlock, 1=lock, 2=schedule, 3=emergency_unlock */
    uint8_t  schedule_id;   /* 0=normal, 1=night, 2=override */
    uint16_t duration_s;    /* Override duration (0=permanent) */
    uint8_t  priority;      /* 0=normal, 1=high, 2=emergency */
} ws_door_lock_cmd_t;

/* === Reminder Trigger payload (8 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  reminder_id;
    uint8_t  clip_index;    /* Voice clip in W25Q128 flash (0-119) */
    uint8_t  volume;        /* 0-100% */
    uint8_t  repeat_count;  /* 1-3 */
    uint8_t  repeat_delay;  /* seconds */
    uint8_t  tone_before;   /* 0=no, 1=chime */
    uint8_t  reminder_type; /* 0=med, 1=meal, 2=water, 3=appt, 4=orient, 5=custom */
    uint8_t  ack_timeout;   /* seconds before escalation */
} ws_reminder_trigger_t;

/* === Band telemetry (28 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  subtype;         /* 0x01 */
    uint8_t  battery_v;      /* ×0.01V */
    int32_t  gps_lat_e7;     /* Latitude × 1e7 */
    int32_t  gps_lon_e7;     /* Longitude × 1e7 */
    uint8_t  gps_fix;        /* 0=no, 1=fix, 2=estimated */
    uint8_t  activity_class; /* ADLNet output */
    uint8_t  wander_risk;    /* WanderNet risk 0-100 */
    uint8_t  hr_bpm;         /* Heart rate */
    uint16_t hrv_ms;         /* HRV RMSSD */
    uint16_t steps;          /* Steps since last telem */
    uint8_t  geofence_status;/* 0=inside, 1=outside, 2=near */
    uint16_t distance_home_m;/* Distance from home center (m) */
    uint8_t  band_on_wrist;  /* 0=removed, 1=worn */
    uint16_t free_heap;
    int8_t   rssi;
    uint16_t uptime_min;
    uint8_t  sos_pressed;    /* 0=no, 1=pressed since last */
} ws_band_telem_t;

/* === Door telemetry (10 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  subtype;       /* 0x02 */
    uint8_t  battery_v;    /* ×0.01V */
    uint8_t  door_id;
    uint8_t  door_state;   /* 0=closed, 1=open */
    uint8_t  lock_state;   /* 0=unlocked, 1=locked, 2=failed */
    uint8_t  tamper;       /* 0=ok, 1=tampered */
    uint8_t  band_proximity; /* 0=absent, 1=present */
    int8_t   rssi;
    uint16_t uptime_min;
} ws_door_telem_t;

/* === Room telemetry (16 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  subtype;          /* 0x03 */
    uint8_t  battery_v;       /* ×0.01V */
    uint8_t  room_id;
    uint8_t  presence;        /* 0=empty, 1=present */
    uint8_t  activity_class;  /* ADLNet 0-7 */
    uint8_t  activity_conf;   /* 0-100% */
    uint8_t  motion_level;    /* 0-255 mmWave */
    uint8_t  range_m_x2;      /* Distance × 0.5 m */
    uint8_t  pir_triggered;   /* 0=no, 1=yes */
    uint16_t adlnet_ms;       /* Inference time */
    uint16_t free_heap;
    int8_t   rssi;
    uint16_t uptime_min;
} ws_room_telem_t;

/* === Voice telemetry (12 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  subtype;           /* 0x04 */
    uint8_t  battery_v;        /* ×0.01V */
    uint8_t  speaker_active;   /* 0=off, 1=playing */
    uint8_t  last_keyword;     /* 0-10, 0xFF=none */
    uint8_t  reminders_24h;    /* Count in last 24h */
    uint8_t  ack_rate;         /* Acknowledgment rate 0-100% */
    uint16_t free_heap;
    int8_t   rssi;
    uint16_t uptime_min;
} ws_voice_telem_t;

/* === Geofence update payload (12 bytes) === */
typedef struct __attribute__((packed)) {
    int32_t  center_lat_e7;   /* Geofence center latitude × 1e7 */
    int32_t  center_lon_e7;   /* Geofence center longitude × 1e7 */
    uint16_t radius_m;        /* Geofence radius (m) */
    uint16_t night_radius_m;  /* Night geofence radius (m) */
    uint8_t  night_start_h;   /* Night start hour */
    uint8_t  night_end_h;     /* Night end hour */
} ws_geofence_t;

/* === Function prototypes === */
size_t ws_encode(const ws_message_t *msg, uint8_t *buf, size_t buf_len);
int    ws_decode(ws_message_t *msg, const uint8_t *buf, size_t len);
uint16_t ws_crc16_ccitt(const uint8_t *data, size_t len);

/* Payload builders */
int ws_build_wander_alert(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                          uint8_t alert_type, int32_t lat_e7, int32_t lon_e7,
                          uint8_t wander_risk, uint8_t activity,
                          uint8_t geofence_status, uint8_t battery_v,
                          uint8_t impact_g, uint8_t on_wrist, uint8_t hr);

int ws_build_door_lock_cmd(ws_message_t *msg, uint8_t src, uint8_t dst,
                           uint16_t msg_id, uint8_t door_id, uint8_t action,
                           uint8_t schedule_id, uint16_t duration_s,
                           uint8_t priority);

int ws_build_reminder_trigger(ws_message_t *msg, uint8_t src, uint8_t dst,
                              uint16_t msg_id, uint8_t reminder_id,
                              uint8_t clip_index, uint8_t volume,
                              uint8_t repeat_count, uint8_t repeat_delay,
                              uint8_t tone_before, uint8_t reminder_type,
                              uint8_t ack_timeout);

int ws_build_band_telem(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                        const ws_band_telem_t *telem);

int ws_build_door_telem(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                        const ws_door_telem_t *telem);

int ws_build_room_telem(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                        const ws_room_telem_t *telem);

int ws_build_voice_telem(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                         const ws_voice_telem_t *telem);

int ws_build_geofence_update(ws_message_t *msg, uint8_t src, uint8_t dst,
                             uint16_t msg_id, const ws_geofence_t *gf);

int ws_build_heartbeat(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                       uint8_t battery_v, int8_t rssi, uint16_t uptime_min);

int ws_build_command(ws_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_id, uint8_t cmd_type, const uint8_t *params,
                     uint8_t param_len);

int ws_build_join_req(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                      uint8_t node_type, uint8_t battery_v,
                      uint8_t fw_major, uint8_t fw_minor);

#endif /* WANDERSYNC_PROTOCOL_H */