/*
 * StormSync — Protocol Header
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#ifndef STORMSYNC_PROTOCOL_H
#define STORMSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define SS_SYNC0            0x5C
#define SS_SYNC1            0xC5
#define SS_BROADCAST         0xFF
#define SS_MAX_PAYLOAD       240
#define SS_MAX_MSG           256
#define SS_MAX_NODES         19
#define SS_AES_KEY_LEN       16
#define SS_CRC_POLY          0x1021  /* CRC-16-CCITT */

/* Message types */
enum ss_msg_type {
    SS_MSG_JOIN_REQ     = 0x01,
    SS_MSG_JOIN_ACK     = 0x02,
    SS_MSG_TELEMETRY    = 0x03,
    SS_MSG_COMMAND      = 0x04,
    SS_MSG_CMD_ACK      = 0x05,
    SS_MSG_ALERT        = 0x06,
    SS_MSG_OTA_BLOCK    = 0x07,
    SS_MSG_OTA_ACK      = 0x08,
    SS_MSG_HEARTBEAT    = 0x09,
    SS_MSG_MESH_RELAY   = 0x0A,
    SS_MSG_FLOOD_STATUS = 0x0B,
    SS_MSG_TIME_SYNC    = 0x0C,
    SS_MSG_CONFIG       = 0x0D,
    SS_MSG_CONFIG_ACK   = 0x0E,
};

/* Telemetry sub-types */
enum ss_telem_subtype {
    SS_TELEM_SUMP   = 0x01,
    SS_TELEM_SOIL   = 0x02,
    SS_TELEM_WEATHER= 0x03,
    SS_TELEM_ACTUATOR=0x04,
};

/* Alert types */
enum ss_alert_type {
    SS_ALERT_LOW_BATTERY     = 0x01,
    SS_ALERT_HIGH_WATER      = 0x02,
    SS_ALERT_CRITICAL_WATER  = 0x03,
    SS_ALERT_PUMP_FAULT      = 0x04,
    SS_ALERT_PUMP_OVERLOAD   = 0x05,
    SS_ALERT_PUMP_DEGRADATION= 0x06,
    SS_ALERT_POWER_OUTAGE    = 0x07,
    SS_ALERT_VALVE_FAULT     = 0x08,
    SS_ALERT_FLOAT_TRIGGER   = 0x09,
    SS_ALERT_NODE_OFFLINE    = 0x0A,
    SS_ALERT_STORM_IMMINENT  = 0x0B,
    SS_ALERT_SENSOR_ANOMALY  = 0x0C,
};

/* Command sub-types */
enum ss_cmd_type {
    SS_CMD_VALVE_CLOSE   = 0x01,
    SS_CMD_VALVE_OPEN    = 0x02,
    SS_CMD_PUMP_ON       = 0x03,
    SS_CMD_PUMP_OFF      = 0x04,
    SS_CMD_ALARM_ON      = 0x05,
    SS_CMD_ALARM_OFF     = 0x06,
    SS_CMD_STORM_MODE    = 0x07,
    SS_CMD_NORMAL_MODE   = 0x08,
    SS_CMD_SET_CONFIG    = 0x09,
    SS_CMD_REBOOT        = 0x0A,
    SS_CMD_CALIBRATE     = 0x0B,
};

/* Message header (7 bytes) */
typedef struct {
    uint8_t  sync[2];       /* 0x5C, 0xC5 */
    uint8_t  src;           /* Source node ID */
    uint8_t  dst;           /* Destination (0xFF = broadcast) */
    uint8_t  type;          /* Message type */
    uint16_t msg_id;        /* Sequence number */
} ss_header_t;

/* Full message with CRC */
typedef struct {
    ss_header_t header;
    uint8_t  payload[SS_MAX_PAYLOAD];
    uint8_t  payload_len;
    uint16_t crc;
} ss_message_t;

/* CRC-16-CCITT */
uint16_t ss_crc16(const uint8_t *data, size_t len);

/* Encode message into buffer, returns total length (0 on error) */
size_t ss_encode(const ss_message_t *msg, uint8_t *buf, size_t buf_len);

/* Decode buffer into message, returns 0 on success, -1 on error */
int ss_decode(ss_message_t *msg, const uint8_t *buf, size_t len);

/* Build a telemetry message for sump sentinel */
int ss_build_sump_telem(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, uint16_t water_level_mm,
                        uint16_t pump_current, uint8_t pump_status,
                        uint16_t flow_rate, int16_t water_temp_deci,
                        uint16_t vib_rms, uint16_t vib_peak,
                        uint8_t mains_ok, uint16_t pump_runtime_min,
                        int8_t rssi);

/* Build a telemetry message for soil saturation probe */
int ss_build_soil_telem(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_mv, uint16_t moist_15, uint16_t moist_45,
                        uint16_t moist_90, int16_t pore_pressure,
                        int8_t temp_15, int8_t temp_45, int8_t temp_90,
                        uint8_t solar_v, int8_t rssi);

/* Build a telemetry message for weather sentinel */
int ss_build_weather_telem(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t battery_mv, int16_t temp_deci,
                           uint16_t humidity_deci, uint16_t pressure_deci,
                           uint16_t wind_speed_deci, uint16_t wind_dir,
                           uint16_t rain_tips, uint8_t pressure_trend,
                           int8_t rssi);

/* Build a telemetry message for flood actuator */
int ss_build_actuator_telem(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                            uint8_t battery_v, uint8_t valve_status,
                            uint8_t pump_relay, uint8_t float_switch,
                            uint8_t mains_ok, uint8_t alarm_status,
                            uint8_t battery_health, int8_t rssi);

/* Build a command message */
int ss_build_command(ss_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len);

/* Build an alert message */
int ss_build_alert(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len);

/* Build a flood status broadcast */
int ss_build_flood_status(ss_message_t *msg, uint8_t src, uint16_t msg_seq,
                          uint8_t risk_level, uint8_t score,
                          uint16_t predicted_level_mm, uint16_t time_to_flood_min);

#endif /* STORMSYNC_PROTOCOL_H */