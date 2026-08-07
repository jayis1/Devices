/*
 * GuideSync — Protocol Header
 * Binary message encoding/decoding for BLE star network
 */
#ifndef GUIDESYNC_PROTOCOL_H
#define GUIDESYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define GS_SYNC0             0x47  /* 'G' */
#define GS_SYNC1             0x53  /* 'S' */
#define GS_BROADCAST          0xFF
#define GS_MAX_PAYLOAD        240
#define GS_MAX_MSG            256

/* Message types */
enum gs_msg_type {
    GS_MSG_JOIN_REQ     = 0x01,
    GS_MSG_JOIN_ACK     = 0x02,
    GS_MSG_TELEMETRY    = 0x03,
    GS_MSG_COMMAND      = 0x04,
    GS_MSG_CMD_ACK      = 0x05,
    GS_MSG_ALERT        = 0x06,
    GS_MSG_OTA_BLOCK    = 0x07,
    GS_MSG_OTA_ACK      = 0x08,
    GS_MSG_HEARTBEAT    = 0x09,
    GS_MSG_NAV_UPDATE   = 0x0A,
    GS_MSG_SCENE_DESC   = 0x0B,
    GS_MSG_OCR_REQUEST  = 0x0C,
    GS_MSG_OCR_RESULT   = 0x0D,
    GS_MSG_FALL_ALERT   = 0x0E,
    GS_MSG_SOS_ALERT    = 0x0F,
    GS_MSG_BEACON_SCAN  = 0x10,
    GS_MSG_NAV_DEST     = 0x11,
    GS_MSG_CALIBRATION  = 0x12,
    GS_MSG_CALIB_ACK    = 0x13,
    GS_MSG_TIME_SYNC    = 0x14,
};

/* Telemetry sub-types */
enum gs_telem_subtype {
    GS_TELEM_GLASSES = 0x01,
    GS_TELEM_CANE    = 0x02,
    GS_TELEM_BAND    = 0x03,
    GS_TELEM_HUB     = 0x04,
};

/* Alert types */
enum gs_alert_type {
    GS_ALERT_LOW_BATTERY      = 0x01,
    GS_ALERT_OBSTACLE_CRIT    = 0x02,
    GS_ALERT_OBSTACLE_WARN    = 0x03,
    GS_ALERT_DROP_OFF         = 0x04,
    GS_ALERT_STAIRS           = 0x05,
    GS_ALERT_FALL             = 0x06,
    GS_ALERT_SOS              = 0x07,
    GS_ALERT_CROSSWALK_WALK   = 0x08,
    GS_ALERT_CROSSWALK_DONT   = 0x09,
    GS_ALERT_NODE_OFFLINE     = 0x0A,
    GS_ALERT_SENSOR_ANOMALY   = 0x0B,
    GS_ALERT_ARRIVED          = 0x0C,
    GS_ALERT_TEXT_READ        = 0x0D,
    GS_ALERT_FACE_RECOGNIZED  = 0x0E,
};

/* Alert severity */
enum gs_alert_severity {
    GS_SEV_INFO      = 0,
    GS_SEV_WARNING   = 1,
    GS_SEV_CRITICAL  = 2,
    GS_SEV_EMERGENCY = 3,
};

/* Command sub-types */
enum gs_cmd_type {
    GS_CMD_NAV_START      = 0x01,
    GS_CMD_NAV_STOP       = 0x02,
    GS_CMD_NAV_PAUSE      = 0x03,
    GS_CMD_READ_TEXT      = 0x04,
    GS_CMD_DESCRIBE_SCENE = 0x05,
    GS_CMD_WHERE_AM_I     = 0x06,
    GS_CMD_SET_VOLUME     = 0x07,
    GS_CMD_SET_HAPTIC     = 0x08,
    GS_CMD_CALIBRATE      = 0x09,
    GS_CMD_REBOOT         = 0x0A,
    GS_CMD_OTA_START      = 0x0B,
    GS_CMD_OTA_COMMIT     = 0x0C,
    GS_CMD_SOS_CANCEL     = 0x0D,
    GS_CMD_FACE_ADD       = 0x0E,
    GS_CMD_FACE_ENABLE    = 0x0F,
    GS_CMD_BEEP           = 0x10,
};

/* Navigation directions (for haptic band) */
enum gs_nav_direction {
    GS_NAV_STRAIGHT  = 0,
    GS_NAV_LEFT      = 1,
    GS_NAV_RIGHT     = 2,
    GS_NAV_TURN_ARND = 3,
    GS_NAV_STOP      = 4,
    GS_NAV_ARRIVED   = 5,
    GS_NAV_UPSTAIRS  = 6,
    GS_NAV_DOWNSTAIRS= 7,
};

/* Message header (7 bytes) */
typedef struct {
    uint8_t  sync[2];       /* 0x47, 0x53 */
    uint8_t  src;           /* Source node ID */
    uint8_t  dst;           /* Destination (0xFF = broadcast) */
    uint8_t  type;          /* Message type */
    uint16_t msg_id;        /* Sequence number */
} gs_header_t;

/* Full message */
typedef struct {
    gs_header_t header;
    uint8_t  payload[GS_MAX_PAYLOAD];
    uint8_t  payload_len;
} gs_message_t;

/* Encode message into buffer, returns total length (0 on error) */
size_t gs_encode(const gs_message_t *msg, uint8_t *buf, size_t buf_len);

/* Decode buffer into message, returns 0 on success, -1 on error */
int gs_decode(gs_message_t *msg, const uint8_t *buf, size_t len);

/* Build glasses telemetry (28 bytes payload) */
int gs_build_glasses_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t battery_v, int8_t head_pitch, int8_t head_roll,
                           int8_t head_yaw, uint8_t obstacle_class,
                           uint8_t obstacle_dist_dm, uint8_t obstacle_dir,
                           uint8_t scene_obj_count, uint8_t primary_obj_class,
                           uint8_t primary_obj_dist_dm, uint8_t crosswalk_detected,
                           uint8_t signal_state, uint8_t countdown_sec,
                           uint8_t tof_min_dist_dm, uint8_t tof_hazard_flag,
                           uint8_t audio_vol, uint8_t bone_conduction_active,
                           uint16_t step_count_24h, int8_t imu_temp,
                           uint16_t scenenet_ms, uint16_t crosswalknet_ms,
                           uint16_t free_heap, int8_t ble_rssi,
                           uint16_t uptime_min, uint8_t tof_valid_zones);

/* Build cane telemetry (14 bytes payload) */
int gs_build_cane_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, uint8_t us_dist_dm, uint8_t us_valid,
                        uint8_t tof_down_dm, uint8_t dropoff_detected,
                        uint8_t stair_detected, uint16_t swing_count_24h,
                        int8_t imu_temp, uint8_t haptic_last, uint8_t haptic_active,
                        int8_t cane_tilt_deg, uint16_t step_count_24h, int8_t ble_rssi);

/* Build haptic band telemetry (12 bytes payload) */
int gs_build_band_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, int8_t imu_temp,
                        uint16_t step_count_24h, uint8_t fall_count_24h,
                        uint8_t haptic_last, uint8_t nav_direction,
                        uint8_t nav_distance_m, uint8_t sos_armed,
                        int8_t ble_rssi, uint16_t uptime_min);

/* Build a command message */
int gs_build_command(gs_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len);

/* Build an alert message */
int gs_build_alert(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len);

/* Build a navigation update (6 bytes payload) */
int gs_build_nav_update(gs_message_t *msg, uint8_t src, uint8_t dst,
                        uint16_t msg_seq, uint8_t direction,
                        uint8_t distance_m, uint16_t landmark_id,
                        uint8_t eta_min, uint8_t step_index);

/* Build a scene description (variable payload) */
int gs_build_scene_desc(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t obj_count, const uint8_t *obj_classes,
                        const uint8_t *obj_distances_dm, const int8_t *obj_directions,
                        uint8_t crosswalk_state, uint8_t text_len, const char *text);

/* Build a fall alert (8 bytes payload) */
int gs_build_fall_alert(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t impact_mg, int8_t temp_c, uint16_t step_count,
                        uint8_t battery_v, uint8_t stillness_sec);

/* Build an SOS alert (4 bytes payload) */
int gs_build_sos_alert(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                       uint8_t battery_v, uint8_t press_duration_s);

/* Build a beacon scan result (variable, 4 + 5×N) */
int gs_build_beacon_scan(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                         uint8_t beacon_count, const uint16_t *beacon_uuids,
                         const int8_t *rssi_vals, const uint8_t *dist_dm);

#endif /* GUIDESYNC_PROTOCOL_H */