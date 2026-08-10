/*
 * FireSync — Protocol Header
 * Binary message encoding/decoding for Sub-GHz TDMA mesh
 */
#ifndef FIRESYNC_PROTOCOL_H
#define FIRESYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define FS_SYNC0             0x46  /* 'F' */
#define FS_SYNC1             0x53  /* 'S' */
#define FS_BROADCAST         0xFF
#define FS_MAX_PAYLOAD       240
#define FS_MAX_MSG           256
#define FS_HEADER_LEN        7      /* sync(2)+src(1)+dst(1)+type(1)+msgid(2) */
#define FS_CRC_LEN           2      /* CRC-16-CCITT */

/* Message types */
enum fs_msg_type {
    FS_MSG_JOIN_REQ      = 0x01,
    FS_MSG_JOIN_ACK      = 0x02,
    FS_MSG_TELEMETRY     = 0x03,
    FS_MSG_COMMAND       = 0x04,
    FS_MSG_CMD_ACK       = 0x05,
    FS_MSG_FIRE_ALERT    = 0x06,   /* EMERGENCY priority */
    FS_MSG_FIRE_CONFIRM  = 0x07,   /* EMERGENCY — Hub confirms fire to all */
    FS_MSG_ALARM_TRIGGER = 0x08,   /* EMERGENCY — sound all alarms */
    FS_MSG_ALARM_STOP    = 0x09,
    FS_MSG_ESCAPE_UPDATE = 0x0A,   /* EMERGENCY — escape route to controller */
    FS_MSG_STOVE_SHUTOFF = 0x0B,
    FS_MSG_PANEL_SHUTOFF = 0x0C,
    FS_MSG_HVAC_SHUTOFF  = 0x0D,
    FS_MSG_DOOR_RELEASE  = 0x0E,   /* EMERGENCY */
    FS_MSG_OTA_BLOCK     = 0x0F,
    FS_MSG_OTA_ACK       = 0x10,
    FS_MSG_HEARTBEAT     = 0x11,
    FS_MSG_OCCUPANT      = 0x12,
    FS_MSG_FIRE_DISPATCH = 0x13,   /* EMERGENCY — 911 dispatch */
    FS_MSG_SUPPRESS_STAT = 0x14,
    FS_MSG_CALIBRATION   = 0x15,
    FS_MSG_CALIB_ACK     = 0x16,
    FS_MSG_TIME_SYNC     = 0x17,
    FS_MSG_SILENCE       = 0x18,
    FS_MSG_TEST_ALARM    = 0x19,
};

/* Telemetry sub-types */
enum fs_telem_subtype {
    FS_TELEM_SENTINEL = 0x01,
    FS_TELEM_STOVE    = 0x02,
    FS_TELEM_PANEL    = 0x03,
    FS_TELEM_ESCAPE   = 0x04,
    FS_TELEM_HUB      = 0x05,
};

/* Fire classes (FlameNet output) */
enum fs_fire_class {
    FS_FIRE_NORMAL     = 0,
    FS_FIRE_COOKING    = 1,
    FS_FIRE_STEAM      = 2,
    FS_FIRE_CIGARETTE  = 3,
    FS_FIRE_CANDLE     = 4,
    FS_FIRE_SMOLDERING = 5,
    FS_FIRE_FLAMING    = 6,
};

/* Alert types */
enum fs_alert_type {
    FS_ALERT_LOW_BATTERY    = 0x01,
    FS_ALERT_SMOKE_WARN      = 0x02,
    FS_ALERT_SMOKE_FIRE      = 0x03,
    FS_ALERT_CO_WARN         = 0x04,
    FS_ALERT_CO_DANGER       = 0x05,
    FS_ALERT_CO_CRITICAL     = 0x06,
    FS_ALERT_THERMAL_WARN    = 0x07,
    FS_ALERT_THERMAL_FIRE    = 0x08,
    FS_ALERT_ARC_FAULT       = 0x09,
    FS_ALERT_OVERLOAD        = 0x0A,
    FS_ALERT_STOVE_SHUTOFF   = 0x0B,
    FS_ALERT_PANEL_TRIP      = 0x0C,
    FS_ALERT_NODE_OFFLINE    = 0x0D,
    FS_ALERT_SENSOR_ANOMALY  = 0x0E,
    FS_ALERT_FIRE_DISPATCH   = 0x0F,
    FS_ALERT_TEST_ALARM      = 0x10,
};

/* Alert severity */
enum fs_alert_severity {
    FS_SEV_INFO      = 0,
    FS_SEV_WARNING  = 1,
    FS_SEV_CRITICAL  = 2,
    FS_SEV_EMERGENCY = 3,
};

/* Command sub-types */
enum fs_cmd_type {
    FS_CMD_SILENCE_ALARM  = 0x01,
    FS_CMD_TEST_ALARM      = 0x02,
    FS_CMD_CLOSE_VALVE     = 0x03,
    FS_CMD_OPEN_VALVE      = 0x04,
    FS_CMD_TRIP_BREAKER    = 0x05,
    FS_CMD_RESET_BREAKER   = 0x06,
    FS_CMD_HVAC_OFF        = 0x07,
    FS_CMD_RELEASE_DOORS   = 0x08,
    FS_CMD_OTA_START       = 0x09,
    FS_CMD_REBOOT          = 0x0A,
    FS_CMD_SET_ROUTE       = 0x0B,
};

/* Message header */
typedef struct {
    uint8_t  src;
    uint8_t  dst;
    uint8_t  type;
    uint16_t msg_id;
} fs_header_t;

/* Full message */
typedef struct {
    fs_header_t header;
    uint8_t  payload[FS_MAX_PAYLOAD];
    uint8_t  payload_len;
} fs_message_t;

/* === Fire Alert payload (12 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  fire_class;       /* FlameNet class (5=smoldering, 6=flaming) */
    uint8_t  confidence;       /* FlameNet confidence (0-100%) */
    uint8_t  room_id;          /* Room identifier (0-15) */
    uint16_t smoke_pm25;       /* Smoke density at detection (μg/m³) */
    uint16_t co_ppm;           /* CO at detection (ppm) */
    int16_t  temp_c_x10;       /* Temperature at detection (×0.1°C) */
    int16_t  thermal_max_c_x10; /* Thermal array max (×0.1°C) */
    uint8_t  occupant;         /* PIR occupant (0=no, 1=yes) */
} fs_fire_alert_t;

/* === Escape Route payload (8 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  fire_room;     /* Room ID where fire detected */
    uint8_t  safe_exit;     /* Recommended exit (0=front, 1=back, 2=garage, 3=window) */
    uint16_t avoid_rooms;   /* Bitmask of rooms to avoid */
    uint8_t  led_path_mask; /* LED strips to activate green */
    uint8_t  led_red_mask;  /* LED strips to activate red */
    uint8_t  voice_msg_id;  /* Voice guidance message ID (0-11) */
    uint8_t  door_mask;     /* Doors to release (bitmask) */
} fs_escape_route_t;

/* === Sentinel telemetry (24 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  subtype;         /* 0x01 */
    uint8_t  battery_v;      /* ×0.01V */
    uint16_t smoke_pm25;     /* μg/m³ */
    uint16_t co_ppm;         /* ppm */
    int16_t  temp_c_x10;     /* ×0.1°C */
    int8_t   temp_rate;      /* °C/min */
    int16_t  thermal_max_x10;/* MLX90640 max (×0.1°C) */
    int16_t  thermal_mean_x10;/* MLX90640 mean (×0.1°C) */
    uint8_t  flame_class;    /* FlameNet output */
    uint8_t  flame_conf;     /* FlameNet confidence % */
    uint8_t  pir_occupant;   /* 0=empty, 1=occupied */
    uint16_t flamenet_ms;    /* Inference time ms */
    uint8_t  thermal_anom;   /* ThermalAnomaly score (0-255) */
    uint16_t free_heap;      /* bytes */
    int8_t   rssi;           /* dBm */
    uint16_t uptime_min;     /* minutes */
} fs_sentinel_telem_t;

/* === Stove telemetry (18 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  subtype;          /* 0x02 */
    uint8_t  battery_v;       /* ×0.01V */
    int16_t  thermal_max_x10;  /* MLX90640 max pan temp (×0.1°C) */
    int16_t  thermal_mean_x10; /* MLX90640 mean (×0.1°C) */
    uint8_t  knob_positions;   /* Bitmask: bits 0-3 on/off, bits 4-7 level 0-3 */
    uint8_t  pantemp_class;    /* PanTemp CNN output */
    uint16_t timer_remaining_s;/* Auto-shutoff timer seconds */
    uint8_t  valve_state;      /* 0=open, 1=closed */
    uint8_t  buzzer_active;    /* 0=off, 1=on */
    uint16_t free_heap;        /* bytes */
    int8_t   rssi;             /* dBm */
    uint16_t uptime_min;       /* minutes */
} fs_stove_telem_t;

/* === Panel telemetry (16 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  subtype;          /* 0x03 */
    uint8_t  battery_v;       /* ×0.01V */
    uint16_t main_current_x100;/* Main current (×0.01A) */
    uint16_t voltage_x10;     /* Mains voltage (×0.1V) */
    uint16_t power_x10;       /* Real power (×0.1W) */
    int8_t   bus_bar_temp_c;  /* °C */
    int8_t   breaker_temp_c;  /* °C */
    uint8_t  arc_fault_class; /* ArcDetect output */
    uint8_t  arc_confidence;  /* 0-100% */
    uint8_t  shunt_tripped;   /* 0=normal, 1=tripped */
    int8_t   rssi;            /* dBm */
    uint16_t uptime_min;      /* minutes */
} fs_panel_telem_t;

/* === Escape telemetry (12 bytes) === */
typedef struct __attribute__((packed)) {
    uint8_t  subtype;          /* 0x04 */
    uint8_t  battery_v;       /* ×0.01V */
    uint8_t  led_zones;       /* Active LED zone bitmask */
    uint8_t  speaker_active;  /* 0=off, 1=on */
    uint8_t  doors_released;  /* Door release bitmask */
    uint8_t  route_active;    /* 0=inactive, 1=active */
    uint16_t free_heap;        /* bytes */
    int8_t   rssi;             /* dBm */
    uint16_t uptime_min;      /* minutes */
} fs_escape_telem_t;

/* === Function prototypes === */
size_t fs_encode(const fs_message_t *msg, uint8_t *buf, size_t buf_len);
int    fs_decode(fs_message_t *msg, const uint8_t *buf, size_t len);
uint16_t fs_crc16_ccitt(const uint8_t *data, size_t len);

/* Payload builders */
int fs_build_fire_alert(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                        uint8_t fire_class, uint8_t confidence, uint8_t room_id,
                        uint16_t smoke_pm25, uint16_t co_ppm,
                        int16_t temp_c_x10, int16_t thermal_max_c_x10,
                        uint8_t occupant);

int fs_build_escape_route(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                          uint8_t fire_room, uint8_t safe_exit,
                          uint16_t avoid_rooms, uint8_t led_path_mask,
                          uint8_t led_red_mask, uint8_t voice_msg_id,
                          uint8_t door_mask);

int fs_build_sentinel_telem(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                            const fs_sentinel_telem_t *telem);

int fs_build_stove_telem(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                         const fs_stove_telem_t *telem);

int fs_build_panel_telem(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                         const fs_panel_telem_t *telem);

int fs_build_escape_telem(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                          const fs_escape_telem_t *telem);

int fs_build_heartbeat(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                       uint8_t battery_v, int8_t rssi, uint16_t uptime_min);

int fs_build_command(fs_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_id, uint8_t cmd_type, const uint8_t *params,
                     uint8_t param_len);

int fs_build_join_req(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                      uint8_t node_type, uint8_t battery_v,
                      uint8_t fw_version_major, uint8_t fw_version_minor);

#endif /* FIRESYNC_PROTOCOL_H */