/*
 * LawnSync — Protocol Header
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#ifndef LAWNSYNC_PROTOCOL_H
#define LAWNSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define LS_SYNC0            0xA5
#define LS_SYNC1             0x5A
#define LS_BROADCAST         0xFF
#define LS_MAX_PAYLOAD       240
#define LS_MAX_MSG           256
#define LS_MAX_NODES         35
#define LS_AES_KEY_LEN       16
#define LS_CRC_POLY          0x1021  /* CRC-16-CCITT */

/* Message types */
enum ls_msg_type {
    LS_MSG_JOIN_REQ    = 0x01,
    LS_MSG_JOIN_ACK    = 0x02,
    LS_MSG_TELEMETRY   = 0x03,
    LS_MSG_COMMAND     = 0x04,
    LS_MSG_CMD_ACK     = 0x05,
    LS_MSG_ALERT       = 0x06,
    LS_MSG_OTA_BLOCK   = 0x07,
    LS_MSG_OTA_ACK     = 0x08,
    LS_MSG_HEARTBEAT   = 0x09,
    LS_MSG_MESH_RELAY  = 0x0A,
    LS_MSG_SCAN_RESULT = 0x0B,
    LS_MSG_TIME_SYNC   = 0x0C,
    LS_MSG_CONFIG      = 0x0D,
    LS_MSG_CONFIG_ACK  = 0x0E,
};

/* Telemetry sub-types */
enum ls_telem_subtype {
    LS_TELEM_SOIL      = 0x01,
    LS_TELEM_WEATHER   = 0x02,
    LS_TELEM_SPRINKLER = 0x03,
    LS_TELEM_SCANNER   = 0x04,
};

/* Alert types */
enum ls_alert_type {
    LS_ALERT_LOW_BATTERY   = 0x01,
    LS_ALERT_LOW_MOISTURE   = 0x02,
    LS_ALERT_HIGH_MOISTURE  = 0x03,
    LS_ALERT_LEAK_DETECTED  = 0x04,
    LS_ALERT_OVERPRESSURE   = 0x05,
    LS_ALERT_FREEZE         = 0x06,
    LS_ALERT_VALVE_FAULT    = 0x07,
    LS_ALERT_DISEASE        = 0x08,
    LS_ALERT_TAMPER         = 0x09,
    LS_ALERT_NODE_OFFLINE   = 0x0A,
};

/* Command sub-types */
enum ls_cmd_type {
    LS_CMD_VALVE_OPEN   = 0x01,
    LS_CMD_VALVE_CLOSE  = 0x02,
    LS_CMD_SCAN_CAPTURE = 0x03,
    LS_CMD_SET_CONFIG   = 0x04,
    LS_CMD_REBOOT       = 0x05,
    LS_CMD_CALIBRATE    = 0x06,
};

/* Message header (6 bytes) */
typedef struct {
    uint8_t  sync[2];       /* 0xA5, 0x5A */
    uint8_t  src;           /* Source node ID */
    uint8_t  dst;           /* Destination (0xFF = broadcast) */
    uint8_t  type;          /* Message type */
    uint16_t msg_id;        /* Sequence number */
} ls_header_t;

/* Full message with CRC */
typedef struct {
    ls_header_t header;
    uint8_t  payload[LS_MAX_PAYLOAD];
    uint8_t  payload_len;
    uint16_t crc;
} ls_message_t;

/* CRC-16-CCITT */
uint16_t ls_crc16(const uint8_t *data, size_t len);

/* Encode message into buffer, returns total length (0 on error) */
size_t ls_encode(const ls_message_t *msg, uint8_t *buf, size_t buf_len);

/* Decode buffer into message, returns 0 on success, -1 on error */
int ls_decode(ls_message_t *msg, const uint8_t *buf, size_t len);

/* Build a telemetry message for soil node */
int ls_build_soil_telem(ls_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_mv, uint16_t moisture_pct,
                        int16_t temp_deci, uint8_t pH_deci,
                        uint16_t nitrogen, uint16_t phosphorus,
                        uint16_t potassium, uint16_t light_lux,
                        uint8_t solar_v, int8_t rssi);

/* Build a telemetry message for weather station */
int ls_build_weather_telem(ls_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                            uint8_t battery_mv, int16_t temp_deci,
                            uint16_t humidity_deci, uint16_t pressure_deci,
                            uint16_t wind_speed_deci, uint16_t wind_dir,
                            uint16_t rain_tips, uint16_t solar_irr,
                            uint8_t uv_index_deci, int8_t rssi);

/* Build a telemetry message for sprinkler controller */
int ls_build_sprinkler_telem(ls_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                              uint8_t active_zone, uint16_t flow_rate,
                              uint32_t total_flow, uint16_t pressure,
                              uint8_t rain_detected, uint8_t valve_status,
                              int8_t rssi);

/* Build a command message */
int ls_build_command(ls_message_t *msg, uint8_t src, uint8_t dst,
                      uint16_t msg_seq, uint8_t cmd_type,
                      const uint8_t *cmd_data, uint8_t cmd_len);

/* Build an alert message */
int ls_build_alert(ls_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                    uint8_t alert_type, uint8_t severity,
                    const uint8_t *alert_data, uint8_t data_len);

#endif /* LAWNSYNC_PROTOCOL_H */