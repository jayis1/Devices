/*
 * VoiceSync — Protocol Header
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#ifndef VOICESYNC_PROTOCOL_H
#define VOICESYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define VS_SYNC0            0x56  /* 'V' */
#define VS_SYNC1            0x53  /* 'S' */
#define VS_BROADCAST         0xFF
#define VS_MAX_PAYLOAD       240
#define VS_MAX_MSG           256
#define VS_MAX_NODES         16
#define VS_AES_KEY_LEN       16
#define VS_CRC_POLY          0x1021  /* CRC-16-CCITT */

/* Message types */
enum vs_msg_type {
    VS_MSG_JOIN_REQ      = 0x01,
    VS_MSG_JOIN_ACK      = 0x02,
    VS_MSG_TELEMETRY     = 0x03,
    VS_MSG_COMMAND       = 0x04,
    VS_MSG_CMD_ACK       = 0x05,
    VS_MSG_ALERT         = 0x06,
    VS_MSG_OTA_BLOCK     = 0x07,
    VS_MSG_OTA_ACK       = 0x08,
    VS_MSG_HEARTBEAT     = 0x09,
    VS_MSG_MESH_RELAY    = 0x0A,
    VS_MSG_VOICE_STATUS  = 0x0B,
    VS_MSG_TIME_SYNC     = 0x0C,
    VS_MSG_CONFIG        = 0x0D,
    VS_MSG_CONFIG_ACK    = 0x0E,
    VS_MSG_VOICE_ALERT   = 0x0F,
};

/* Telemetry sub-types */
enum vs_telem_subtype {
    VS_TELEM_VOCAL_BAND  = 0x01,
    VS_TELEM_ROOM        = 0x02,
    VS_TELEM_HYDRATION   = 0x03,
    VS_TELEM_HUMIDITY    = 0x04,
};

/* Alert types */
enum vs_alert_type {
    VS_ALERT_LOW_BATTERY      = 0x01,
    VS_ALERT_VOCAL_REST       = 0x02,
    VS_ALERT_HIGH_RISK        = 0x03,
    VS_ALERT_HOARSENESS       = 0x04,
    VS_ALERT_REFLUX           = 0x05,
    VS_ALERT_LOW_HUMIDITY     = 0x06,
    VS_ALERT_DEHYDRATION      = 0x07,
    VS_ALERT_VOCAL_ANOMALY    = 0x08,
    VS_ALERT_NODE_OFFLINE     = 0x09,
    VS_ALERT_SENSOR_ANOMALY   = 0x0A,
    VS_ALERT_TANK_EMPTY       = 0x0B,
    VS_ALERT_POOR_POSTURE     = 0x0C,
};

/* Command sub-types */
enum vs_cmd_type {
    VS_CMD_HUMIDIFIER_ON  = 0x01,
    VS_CMD_HUMIDIFIER_OFF = 0x02,
    VS_CMD_FAN_ON         = 0x03,
    VS_CMD_FAN_OFF        = 0x04,
    VS_CMD_BUZZER_ON      = 0x05,
    VS_CMD_BUZZER_OFF     = 0x06,
    VS_CMD_HIGH_RISK_MODE = 0x07,
    VS_CMD_NORMAL_MODE    = 0x08,
    VS_CMD_SET_CONFIG     = 0x09,
    VS_CMD_REBOOT         = 0x0A,
    VS_CMD_CALIBRATE      = 0x0B,
    VS_CMD_START_RECORD   = 0x0C,
    VS_CMD_STOP_RECORD    = 0x0D,
};

/* Voice quality classes (VoiceNet) */
enum vs_voice_class {
    VS_VOICE_NORMAL   = 0,
    VS_VOICE_HOARSE   = 1,
    VS_VOICE_BREATHY  = 2,
    VS_VOICE_STRAINED = 3,
    VS_VOICE_TREMOR   = 4,
    VS_VOICE_FATIGUE  = 5,
    VS_VOICE_REFLUX   = 6,
    VS_VOICE_DISORDER = 7,
};

/* Voice quality names for reference */
#define VS_VOICE_CLASS_NAMES \
    { "Normal", "Hoarse", "Breathy", "Strained", "Tremor", "Fatigue", "Reflux", "Disorder" }

/* Classes that indicate clinical concern */
#define IS_VOICE_DISORDER_CLASS(c) ((c) >= 1 && (c) != 5)  /* Hoarse, Breathy, Strained, Tremor, Reflux, Disorder */
#define IS_VOICE_CRITICAL_CLASS(c) ((c) == 1 || (c) == 4 || (c) == 6 || (c) == 7) /* Hoarse, Tremor, Reflux, Disorder */

/* Message header (7 bytes) */
typedef struct {
    uint8_t  sync[2];       /* 0x56, 0x53 */
    uint8_t  src;           /* Source node ID */
    uint8_t  dst;           /* Destination (0xFF = broadcast) */
    uint8_t  type;          /* Message type */
    uint16_t msg_id;        /* Sequence number */
} vs_header_t;

/* Full message with CRC */
typedef struct {
    vs_header_t header;
    uint8_t  payload[VS_MAX_PAYLOAD];
    uint8_t  payload_len;
    uint16_t crc;
} vs_message_t;

/* CRC-16-CCITT */
uint16_t vs_crc16(const uint8_t *data, size_t len);

/* Encode message into buffer, returns total length (0 on error) */
size_t vs_encode(const vs_message_t *msg, uint8_t *buf, size_t buf_len);

/* Decode buffer into message, returns 0 on success, -1 on error */
int vs_decode(vs_message_t *msg, const uint8_t *buf, size_t len);

/* Build a telemetry message for vocal band (22 bytes payload) */
int vs_build_vocal_band_telem(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                              uint8_t battery_v, uint16_t f0_deci,
                              uint16_t jitter_centi, uint16_t shimmer_centi,
                              int8_t hnr_db, uint8_t phonation_pct,
                              uint8_t intensity_db, uint16_t pitch_range_deci,
                              int16_t neck_angle_deci, uint16_t skin_temp_centi,
                              uint8_t heart_rate, uint8_t hrv_rmssd,
                              uint8_t stress_level, int8_t rssi);

/* Build a telemetry message for room sentinel (16 bytes payload) */
int vs_build_room_telem(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, uint8_t voice_quality_class,
                        uint8_t confidence_pct, uint16_t f0_deci,
                        uint8_t phonation_pct, int16_t temp_deci,
                        uint16_t humidity_deci, uint16_t voc_index,
                        uint8_t db_spl, uint8_t talking_detected,
                        int8_t rssi);

/* Build a telemetry message for hydration tag (10 bytes payload) */
int vs_build_hydration_telem(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                             uint8_t battery_v, uint16_t water_mass_g,
                             uint16_t sips_24h, uint16_t intake_ml,
                             uint8_t last_sip_min, int8_t rssi);

/* Build a telemetry message for humidity node (10 bytes payload) */
int vs_build_humidity_telem(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                            uint8_t battery_v, int16_t temp_deci,
                            uint16_t humidity_deci, uint8_t tank_level_pct,
                            uint8_t humidifier_on, uint8_t fan_on,
                            int8_t rssi);

/* Build a command message */
int vs_build_command(vs_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len);

/* Build an alert message */
int vs_build_alert(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len);

/* Build a voice status broadcast */
int vs_build_voice_status(vs_message_t *msg, uint8_t src, uint16_t msg_seq,
                          uint8_t risk_level, uint8_t vocal_health_score,
                          uint8_t disorder_risk, uint8_t phonation_pct_today,
                          uint8_t hydration_pct, uint8_t rest_recommended,
                          uint16_t rest_minutes_remaining);

/* Build a voice alert (immediate voice quality notification) */
int vs_build_voice_alert(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                         uint8_t voice_quality_class, uint8_t confidence,
                         uint16_t f0_deci, uint8_t is_critical);

#endif /* VOICESYNC_PROTOCOL_H */