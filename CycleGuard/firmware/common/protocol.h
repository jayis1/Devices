#ifndef CYCLEGUARD_PROTOCOL_H
#define CYCLEGUARD_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* Node IDs */
#define NODE_ID_HUB            0x0001
#define NODE_ID_SMART_HELMET   0x0002
#define NODE_ID_SMART_LIGHT    0x0003
#define NODE_ID_BIKE_SENSOR    0x0004
#define NODE_ID_SMART_LOCK     0x0005
#define NODE_ID_BROADCAST      0xFFFF

/* Message types */
#define MSG_TYPE_BEACON          0x01
#define MSG_TYPE_SENSOR_DATA     0x02
#define MSG_TYPE_CRASH_ALERT     0x03
#define MSG_TYPE_LOCK_CMD        0x04
#define MSG_TYPE_JOIN_REQ        0x05
#define MSG_TYPE_JOIN_ACK        0x06
#define MSG_TYPE_HEARTBEAT       0x07
#define MSG_TYPE_OTA_CHUNK       0x08
#define MSG_TYPE_CONFIG          0x09
#define MSG_TYPE_CAL_REQ         0x0A
#define MSG_TYPE_PROXIMITY_WARN  0x0B
#define MSG_TYPE_TURN_SIGNAL     0x0C
#define MSG_TYPE_THEFT_ALERT     0x0D
#define MSG_TYPE_LIGHT_CMD       0x0E
#define MSG_TYPE_TIRE_PRESSURE   0x0F

/* Frame constants */
#define FRAME_PREAMBLE_LEN  4
#define FRAME_SYNC_LEN      2
#define FRAME_HEADER_LEN    12
#define FRAME_PAYLOAD_MAX   32
#define FRAME_CRC_LEN       2
#define FRAME_MAX_LEN       (FRAME_HEADER_LEN + FRAME_PAYLOAD_MAX + FRAME_CRC_LEN)

#define SYNC_WORD_0 0x3C
#define SYNC_WORD_1 0xD2

/* Haptic patterns (helmet + handlebar) */
#define HAPTIC_NONE           0x00
#define HAPTIC_SINGLE_TAP     0x01   /* turn approaching */
#define HAPTIC_DOUBLE_PULSE   0x02   /* vehicle behind */
#define HAPTIC_TRIPLE_BURST   0x03   /* crash imminent */
#define HAPTIC_LONG_BUZZ      0x04   /* general alert */
#define HAPTIC_THEFT_ALARM    0x05   /* theft detected */

/* Crash classes (CrashNet) */
#define CRASH_NORMAL          0
#define CRASH_POTHOLE         1
#define CRASH_NEAR_MISS       2
#define CRASH_CRASH           3
#define CRASH_COUNT           4

/* Lock states */
#define LOCK_DISARMED         0
#define LOCK_ARMED            1
#define LOCK_TAMPER           2
#define LOCK_ALARM            3
#define LOCK_TRACKING         4

/* Turn signal states */
#define TURN_NONE             0
#define TURN_LEFT             1
#define TURN_RIGHT            2
#define TURN_HAZARD           3

/* BlindSpot classes (BlindSpotNet) */
#define BLINDSPOT_CLEAR       0
#define BLINDSPOT_BICYCLE     1
#define BLINDSPOT_MOTORCYCLE  2
#define BLINDSPOT_CAR         3
#define BLINDSPOT_TRUCK       4
#define BLINDSPOT_BUS         5
#define BLINDSPOT_PED         6
#define BLINDSPOT_OBSTACLE    7

/* Frame structure */
typedef struct __attribute__((packed)) {
    uint8_t  preamble[FRAME_PREAMBLE_LEN];
    uint8_t  sync[FRAME_SYNC_LEN];
    uint8_t  length;
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t  msg_type;
    uint16_t seq_num;
    uint8_t  payload[FRAME_PAYLOAD_MAX];
    uint16_t crc;
} mesh_frame_t;

/* Sensor data payload variants (32 bytes max) */

/* Helmet payload: crash detection + acoustic + haptic */
typedef struct __attribute__((packed)) {
    uint8_t  crash_class;        /* CRASH_* */
    float    impact_g;           /* peak acceleration in g */
    float    rot_velocity;       /* peak rotational velocity deg/s */
    uint8_t  horn_detected;      /* bool */
    uint8_t  siren_detected;     /* bool */
    uint8_t  hr;                 /* bpm (from helmet PPG if equipped) */
    uint8_t  battery_pct;
    uint8_t  reserved[13];
} helmet_payload_t;

/* Light payload: brake status + turn signal + brightness */
typedef struct __attribute__((packed)) {
    uint8_t  braking;            /* bool */
    uint8_t  turn_signal;        /* TURN_* */
    uint8_t  headlight_pct;      /* 0-100 brightness */
    uint8_t  taillight_pct;      /* 0-100 brightness */
    float    ambient_lux;        /* light sensor reading */
    uint8_t  battery_pct;
    uint8_t  reserved[19];
} light_payload_t;

/* Bike sensor payload: speed + cadence + tire pressure */
typedef struct __attribute__((packed)) {
    float    speed_kmh;          /* wheel speed km/h */
    uint8_t  cadence_rpm;        /* crank cadence RPM */
    float    tire_pressure_psi;  /* TPMS pressure PSI */
    float    tire_temp_c;        /* TPMS temperature */
    uint8_t  battery_pct;
    uint8_t  reserved[15];
} bike_sensor_payload_t;

/* Lock payload: state + GPS + tamper */
typedef struct __attribute__((packed)) {
    uint8_t  lock_state;         /* LOCK_* */
    int32_t  gps_lat_e7;         /* latitude × 1e7 */
    int32_t  gps_lon_e7;         /* longitude × 1e7 */
    uint8_t  tamper_count;       /* tamper events since arm */
    uint16_t load_cell_kg;       /* current load cell force */
    uint8_t  battery_pct;
    uint8_t  reserved[12];
} lock_payload_t;

/* Lock command payload (Hub→Lock) */
typedef struct __attribute__((packed)) {
    uint8_t  command;            /* 0=disarm, 1=arm, 2=silent, 3=alarm */
    uint8_t  geo_fence_m;        /* geo-fence radius in meters */
    uint8_t  reserved[30];
} lock_cmd_payload_t;

/* Light command payload (Hub→Light) */
typedef struct __attribute__((packed)) {
    uint8_t  turn_signal;        /* TURN_* */
    uint8_t  headlight_pct;      /* override brightness (0=auto) */
    uint8_t  mode;               /* 0=normal, 1=hazard, 2=crash, 3=DRL */
    uint8_t  reserved[29];
} light_cmd_payload_t;

/* Function prototypes */
uint16_t protocol_crc16(const uint8_t *data, size_t len);
void protocol_build_frame(mesh_frame_t *frame, uint16_t src, uint16_t dst,
                           uint8_t msg_type, uint16_t seq,
                           const uint8_t *payload, uint8_t payload_len);
bool protocol_parse_frame(const uint8_t *raw, size_t len, mesh_frame_t *out);
bool protocol_validate_crc(const mesh_frame_t *frame);

#endif /* CYCLEGUARD_PROTOCOL_H */