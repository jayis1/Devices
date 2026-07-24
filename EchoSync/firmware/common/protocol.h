/*
 * EchoSync — Protocol Header
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#ifndef ECHOSYNC_PROTOCOL_H
#define ECHOSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define ES_SYNC0            0x45  /* 'E' */
#define ES_SYNC1            0x53  /* 'S' */
#define ES_BROADCAST        0xFF
#define ES_MAX_PAYLOAD      240
#define ES_MAX_MSG          256
#define ES_MAX_NODES        16
#define ES_AES_KEY_LEN      16
#define ES_CRC_POLY         0x1021  /* CRC-16-CCITT */

/* Message types */
enum es_msg_type {
    ES_MSG_JOIN_REQ      = 0x01,
    ES_MSG_JOIN_ACK      = 0x02,
    ES_MSG_TELEMETRY     = 0x03,
    ES_MSG_COMMAND       = 0x04,
    ES_MSG_CMD_ACK       = 0x05,
    ES_MSG_ALERT         = 0x06,
    ES_MSG_OTA_BLOCK     = 0x07,
    ES_MSG_OTA_ACK       = 0x08,
    ES_MSG_HEARTBEAT     = 0x09,
    ES_MSG_MESH_RELAY    = 0x0A,
    ES_MSG_SOUND_EVENT   = 0x0B,
    ES_MSG_TIME_SYNC     = 0x0C,
    ES_MSG_CONFIG        = 0x0D,
    ES_MSG_CONFIG_ACK    = 0x0E,
    ES_MSG_SOUND_ENROLL  = 0x0F,
    ES_MSG_ENROLL_SAMPLE = 0x10,
    ES_MSG_DISPLAY_UPDATE= 0x11,
};

/* Telemetry sub-types */
enum es_telem_subtype {
    ES_TELEM_SENTINEL  = 0x01,
    ES_TELEM_WRIST     = 0x02,
    ES_TELEM_DOOR      = 0x03,
};

/* Alert / sound class types */
enum es_sound_class {
    ES_SOUND_SMOKE_ALARM  = 0x00,
    ES_SOUND_CO_ALARM     = 0x01,
    ES_SOUND_GLASS_BREAK  = 0x02,
    ES_SOUND_SIREN        = 0x03,
    ES_SOUND_DOORBELL     = 0x04,
    ES_SOUND_DOOR_KNOCK   = 0x05,
    ES_SOUND_PHONE_RING   = 0x06,
    ES_SOUND_BABY_CRY     = 0x07,
    ES_SOUND_CAR_HORN     = 0x08,
    ES_SOUND_DOOR_OPEN    = 0x09,
    ES_SOUND_DOOR_CLOSE   = 0x0A,
    ES_SOUND_WATER        = 0x0B,
    ES_SOUND_DOG_BARK     = 0x0C,
    ES_SOUND_ALARM_CLOCK  = 0x0D,
    ES_SOUND_MICROWAVE    = 0x0E,
    ES_SOUND_DISHWASHER   = 0x0F,
    ES_SOUND_WASHING      = 0x10,
    ES_SOUND_PERSON       = 0x11,
    ES_SOUND_CUSTOM_1     = 0x12,
    ES_SOUND_CUSTOM_2     = 0x13,
    ES_SOUND_CLASS_COUNT  = 20,
};

/* Alert types (for ALERT messages) */
enum es_alert_type {
    ES_ALERT_LOW_BATTERY   = 0x01,
    ES_ALERT_SOUND_EVENT    = 0x02,
    ES_ALERT_EMERGENCY      = 0x03,
    ES_ALERT_NODE_OFFLINE   = 0x04,
    ES_ALERT_SENSOR_ANOMALY = 0x05,
    ES_ALERT_ENROLL_DONE    = 0x06,
    ES_ALERT_CUSTOM_LEARNED = 0x07,
};

/* Command sub-types */
enum es_cmd_type {
    ES_CMD_BED_SHAKER_ON   = 0x01,
    ES_CMD_BED_SHAKER_OFF  = 0x02,
    ES_CMD_BUZZER_ON        = 0x03,
    ES_CMD_BUZZER_OFF       = 0x04,
    ES_CMD_EMERGENCY_MODE   = 0x05,
    ES_CMD_NORMAL_MODE      = 0x06,
    ES_CMD_SET_CONFIG       = 0x07,
    ES_CMD_REBOOT           = 0x08,
    ES_CMD_CALIBRATE        = 0x09,
    ES_CMD_START_ENROLL     = 0x0A,
    ES_CMD_STOP_ENROLL      = 0x0B,
    ES_CMD_HAPTIC_PATTERN   = 0x0C,
    ES_CMD_DISPLAY_UPDATE   = 0x0D,
    ES_CMD_SILENCE_ALERTS   = 0x0E,
};

/* Priority levels */
enum es_priority {
    ES_PRIORITY_INFO      = 0,
    ES_PRIORITY_IMPORTANT = 1,
    ES_PRIORITY_EMERGENCY = 2,
};

/* Sound class → priority mapping */
static const uint8_t es_class_priority[20] = {
    /* Smoke */    ES_PRIORITY_EMERGENCY,
    /* CO */       ES_PRIORITY_EMERGENCY,
    /* Glass */    ES_PRIORITY_EMERGENCY,
    /* Siren */    ES_PRIORITY_EMERGENCY,
    /* Doorbell */ ES_PRIORITY_IMPORTANT,
    /* Knock */    ES_PRIORITY_IMPORTANT,
    /* Phone */    ES_PRIORITY_IMPORTANT,
    /* Baby */     ES_PRIORITY_IMPORTANT,
    /* Car horn */ ES_PRIORITY_IMPORTANT,
    /* Door open */ES_PRIORITY_INFO,
    /* Door close*/ES_PRIORITY_INFO,
    /* Water */    ES_PRIORITY_INFO,
    /* Dog bark */ ES_PRIORITY_INFO,
    /* Alarm clk */ ES_PRIORITY_INFO,
    /* Microwave */ES_PRIORITY_INFO,
    /* Dishwasher*/ES_PRIORITY_INFO,
    /* Washing */  ES_PRIORITY_INFO,
    /* Person */   ES_PRIORITY_INFO,
    /* Custom 1 */ ES_PRIORITY_INFO,
    /* Custom 2 */ ES_PRIORITY_INFO,
};

/* Sound class names for reference */
#define ES_SOUND_CLASS_NAMES \
    { "SmokeAlarm","COAlarm","GlassBreak","Siren","Doorbell","DoorKnock", \
      "PhoneRing","BabyCry","CarHorn","DoorOpen","DoorClose","Water", \
      "DogBark","AlarmClock","Microwave","Dishwasher","WashingMachine", \
      "PersonEnter","Custom1","Custom2" }

/* Emergency classes */
#define IS_EMERGENCY_CLASS(c) ((c) <= ES_SOUND_SIREN)
#define IS_IMPORTANT_CLASS(c) ((c) >= ES_SOUND_DOORBELL && (c) <= ES_SOUND_CAR_HORN)

/* Message header (7 bytes) */
typedef struct {
    uint8_t  sync[2];       /* 0x45, 0x53 */
    uint8_t  src;           /* Source node ID */
    uint8_t  dst;           /* Destination (0xFF = broadcast) */
    uint8_t  type;          /* Message type */
    uint16_t msg_id;        /* Sequence number */
} es_header_t;

/* Full message with CRC */
typedef struct {
    es_header_t header;
    uint8_t  payload[ES_MAX_PAYLOAD];
    uint8_t  payload_len;
    uint16_t crc;
} es_message_t;

/* CRC-16-CCITT */
uint16_t es_crc16(const uint8_t *data, size_t len);

/* Encode message into buffer, returns total length (0 on error) */
size_t es_encode(const es_message_t *msg, uint8_t *buf, size_t buf_len);

/* Decode buffer into message, returns 0 on success, -1 on error */
int es_decode(es_message_t *msg, const uint8_t *buf, size_t len);

/* Build a telemetry message for room sentinel (18 bytes payload) */
int es_build_sentinel_telem(es_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                             uint8_t battery_v, uint8_t sound_class,
                             uint8_t confidence, uint16_t direction_deci,
                             int8_t direction_elev, uint16_t duration_ms,
                             int16_t temp_deci, uint16_t humidity_deci,
                             uint8_t db_spl, uint8_t priority,
                             uint16_t event_id, int8_t rssi);

/* Build a telemetry message for wrist band (10 bytes payload) */
int es_build_wrist_telem(es_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                          uint8_t battery_v, uint8_t worn, uint8_t sleeping,
                          uint8_t last_alert_class, uint8_t last_alert_priority,
                          uint16_t alerts_24h, int8_t rssi);

/* Build a telemetry message for door tag (10 bytes payload) */
int es_build_door_telem(es_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, uint8_t event_type,
                        uint8_t confidence, uint8_t knock_count,
                        uint16_t event_id, int8_t rssi);

/* Build a sound event broadcast (Hub→Wrist Band) */
int es_build_sound_event(es_message_t *msg, uint8_t src, uint16_t msg_seq,
                          uint8_t sound_class, uint8_t priority,
                          uint8_t confidence, uint16_t direction_deci,
                          uint8_t source_node, uint16_t room_hash,
                          uint16_t event_id, uint8_t haptic_pattern);

/* Build a command message */
int es_build_command(es_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len);

/* Build an alert message */
int es_build_alert(es_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len);

#endif /* ECHOSYNC_PROTOCOL_H */