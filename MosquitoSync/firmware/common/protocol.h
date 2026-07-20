/*
 * MosquitoSync — Protocol Header
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#ifndef MOSQUITOSYNC_PROTOCOL_H
#define MOSQUITOSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define MS_SYNC0            0x6D  /* 'M' */
#define MS_SYNC1            0x53  /* 'S' */
#define MS_BROADCAST         0xFF
#define MS_MAX_PAYLOAD       240
#define MS_MAX_MSG           256
#define MS_MAX_NODES         24
#define MS_AES_KEY_LEN       16
#define MS_CRC_POLY          0x1021  /* CRC-16-CCITT */

/* Message types */
enum ms_msg_type {
    MS_MSG_JOIN_REQ      = 0x01,
    MS_MSG_JOIN_ACK      = 0x02,
    MS_MSG_TELEMETRY     = 0x03,
    MS_MSG_COMMAND       = 0x04,
    MS_MSG_CMD_ACK       = 0x05,
    MS_MSG_ALERT         = 0x06,
    MS_MSG_OTA_BLOCK     = 0x07,
    MS_MSG_OTA_ACK       = 0x08,
    MS_MSG_HEARTBEAT     = 0x09,
    MS_MSG_MESH_RELAY    = 0x0A,
    MS_MSG_RISK_STATUS   = 0x0B,
    MS_MSG_TIME_SYNC     = 0x0C,
    MS_MSG_CONFIG        = 0x0D,
    MS_MSG_CONFIG_ACK    = 0x0E,
    MS_MSG_SPECIES_ALERT = 0x0F,
};

/* Telemetry sub-types */
enum ms_telem_subtype {
    MS_TELEM_ACOUSTIC = 0x01,
    MS_TELEM_TRAP     = 0x02,
    MS_TELEM_BARRIER  = 0x03,
    MS_TELEM_WEATHER  = 0x04,
};

/* Alert types */
enum ms_alert_type {
    MS_ALERT_LOW_BATTERY     = 0x01,
    MS_ALERT_MOSQUITO_DETECTED= 0x02,
    MS_ALERT_DISEASE_VECTOR  = 0x03,
    MS_ALERT_TRAP_FULL       = 0x04,
    MS_ALERT_PROPANE_LOW     = 0x05,
    MS_ALERT_PROPANE_LEAK    = 0x06,
    MS_ALERT_BARRIER_STUCK   = 0x07,
    MS_ALERT_HIGH_RISK       = 0x08,
    MS_ALERT_DISEASE_OUTBREAK= 0x09,
    MS_ALERT_NODE_OFFLINE    = 0x0A,
    MS_ALERT_SENSOR_ANOMALY  = 0x0B,
    MS_ALERT_HEATER_FAULT    = 0x0C,
};

/* Command sub-types */
enum ms_cmd_type {
    MS_CMD_BARRIER_CLOSE   = 0x01,
    MS_CMD_BARRIER_OPEN    = 0x02,
    MS_CMD_TRAP_CO2_ON     = 0x03,
    MS_CMD_TRAP_CO2_OFF    = 0x04,
    MS_CMD_TRAP_FAN_ON     = 0x05,
    MS_CMD_TRAP_FAN_OFF    = 0x06,
    MS_CMD_ALARM_ON        = 0x07,
    MS_CMD_ALARM_OFF       = 0x08,
    MS_CMD_HIGH_RISK_MODE  = 0x09,
    MS_CMD_NORMAL_MODE     = 0x0A,
    MS_CMD_SET_CONFIG      = 0x0B,
    MS_CMD_REBOOT          = 0x0C,
    MS_CMD_CALIBRATE       = 0x0D,
    MS_CMD_CAPTURE_IMAGE   = 0x0E,
};

/* Message header (7 bytes) */
typedef struct {
    uint8_t  sync[2];       /* 0x6D, 0x53 */
    uint8_t  src;           /* Source node ID */
    uint8_t  dst;           /* Destination (0xFF = broadcast) */
    uint8_t  type;          /* Message type */
    uint16_t msg_id;        /* Sequence number */
} ms_header_t;

/* Full message with CRC */
typedef struct {
    ms_header_t header;
    uint8_t  payload[MS_MAX_PAYLOAD];
    uint8_t  payload_len;
    uint16_t crc;
} ms_message_t;

/* CRC-16-CCITT */
uint16_t ms_crc16(const uint8_t *data, size_t len);

/* Encode message into buffer, returns total length (0 on error) */
size_t ms_encode(const ms_message_t *msg, uint8_t *buf, size_t buf_len);

/* Decode buffer into message, returns 0 on success, -1 on error */
int ms_decode(ms_message_t *msg, const uint8_t *buf, size_t len);

/* Build a telemetry message for acoustic sentinel (14 bytes payload) */
int ms_build_acoustic_telem(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                             uint8_t battery_v, int8_t temp, uint8_t humidity,
                             uint8_t mosquito_detected, uint8_t species_class,
                             uint8_t confidence, uint16_t wingbeat_freq,
                             uint16_t detections_24h, uint16_t audio_energy,
                             int8_t rssi);

/* Build a telemetry message for CO2 trap (20 bytes payload) */
int ms_build_trap_telem(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, int16_t temp_deci, uint16_t humidity_deci,
                        uint16_t pressure_deci, uint16_t rain_tips,
                        uint16_t ir_breaks, uint16_t capture_24h,
                        uint8_t trap_fullness, uint8_t co2_on,
                        uint8_t propane_pct, uint8_t fan_pct,
                        uint8_t dominant_species, int8_t rssi);

/* Build a telemetry message for window barrier (8 bytes payload) */
int ms_build_barrier_telem(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t battery_v, uint8_t screen_status,
                           uint8_t last_trigger, uint8_t cycles_24h,
                           uint16_t motor_current, int8_t rssi);

/* Build a telemetry message for weather sentinel (15 bytes payload) */
int ms_build_weather_telem(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t battery_v, int16_t temp_deci,
                           uint16_t humidity_deci, uint16_t pressure_deci,
                           uint16_t wind_speed_deci, uint16_t wind_dir,
                           uint16_t rain_tips, int8_t rssi);

/* Build a command message */
int ms_build_command(ms_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len);

/* Build an alert message */
int ms_build_alert(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len);

/* Build a risk status broadcast */
int ms_build_risk_status(ms_message_t *msg, uint8_t src, uint16_t msg_seq,
                          uint8_t risk_level, uint8_t bite_risk_score,
                          uint8_t disease_risk_score, uint8_t activity_index,
                          uint16_t predicted_peak_time_min);

/* Build a species alert (immediate mosquito detection notification) */
int ms_build_species_alert(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t species_class, uint8_t confidence,
                           uint16_t wingbeat_freq, uint8_t is_disease_vector);

#endif /* MOSQUITOSYNC_PROTOCOL_H */