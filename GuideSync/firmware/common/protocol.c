/*
 * GuideSync — Protocol Implementation
 * Binary message encoding/decoding for BLE star network
 *
 * Note: BLE link layer provides CRC and AES-128-CCM encryption,
 * so no application-layer CRC is needed (unlike Sub-GHz systems).
 */
#include "protocol.h"

/* Encode message into buffer */
size_t gs_encode(const gs_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (!msg || !buf) return 0;
    size_t total = 7 + msg->payload_len; /* header(7) + payload, no CRC */
    if (total > buf_len || msg->payload_len > GS_MAX_PAYLOAD) return 0;

    size_t idx = 0;
    /* Header */
    buf[idx++] = GS_SYNC0;
    buf[idx++] = GS_SYNC1;
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);

    /* Payload */
    if (msg->payload_len > 0)
        memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    return idx;
}

/* Decode buffer into message */
int gs_decode(gs_message_t *msg, const uint8_t *buf, size_t len)
{
    if (!msg || !buf || len < 7) return -1; /* min: 7 header */

    if (buf[0] != GS_SYNC0 || buf[1] != GS_SYNC1) return -1;

    msg->header.src      = buf[2];
    msg->header.dst      = buf[3];
    msg->header.type     = buf[4];
    msg->header.msg_id   = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);

    msg->payload_len = (uint8_t)(len - 7);
    if (msg->payload_len > GS_MAX_PAYLOAD) return -1;

    if (msg->payload_len > 0)
        memcpy(msg->payload, &buf[7], msg->payload_len);

    return 0;
}

/* Build glasses telemetry payload (28 bytes) */
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
                           uint16_t uptime_min, uint8_t tof_valid_zones)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00; /* Hub */
    msg->header.type = GS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = GS_TELEM_GLASSES;
    p[1]  = battery_v;
    p[2]  = (uint8_t)head_pitch;
    p[3]  = (uint8_t)head_roll;
    p[4]  = (uint8_t)head_yaw;
    p[5]  = obstacle_class;
    p[6]  = obstacle_dist_dm;
    p[7]  = obstacle_dir;
    p[8]  = scene_obj_count;
    p[9]  = primary_obj_class;
    p[10] = primary_obj_dist_dm;
    p[11] = crosswalk_detected;
    p[12] = signal_state;
    p[13] = countdown_sec;
    p[14] = tof_min_dist_dm;
    p[15] = tof_hazard_flag;
    p[16] = audio_vol;
    p[17] = bone_conduction_active;
    p[18] = (uint8_t)(step_count_24h & 0xFF);
    p[19] = (uint8_t)(step_count_24h >> 8);
    p[20] = (uint8_t)imu_temp;
    p[21] = (uint8_t)(scenenet_ms & 0xFF);
    p[22] = (uint8_t)(scenenet_ms >> 8);
    p[23] = (uint8_t)(crosswalknet_ms & 0xFF);
    p[24] = (uint8_t)(crosswalknet_ms >> 8);
    p[25] = (uint8_t)(free_heap & 0xFF);
    p[26] = (uint8_t)(free_heap >> 8);
    p[27] = (uint8_t)ble_rssi;
    /* uptime_min and tof_valid_zones packed in bytes 28-30 */
    p[28] = (uint8_t)(uptime_min & 0xFF);
    p[29] = (uint8_t)(uptime_min >> 8);
    p[30] = tof_valid_zones;
    msg->payload_len = 31;

    return 0;
}

/* Build cane telemetry payload (14 bytes) */
int gs_build_cane_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, uint8_t us_dist_dm, uint8_t us_valid,
                        uint8_t tof_down_dm, uint8_t dropoff_detected,
                        uint8_t stair_detected, uint16_t swing_count_24h,
                        int8_t imu_temp, uint8_t haptic_last, uint8_t haptic_active,
                        int8_t cane_tilt_deg, uint16_t step_count_24h, int8_t ble_rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = GS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = GS_TELEM_CANE;
    p[1]  = battery_v;
    p[2]  = us_dist_dm;
    p[3]  = us_valid;
    p[4]  = tof_down_dm;
    p[5]  = dropoff_detected;
    p[6]  = stair_detected;
    p[7]  = (uint8_t)(swing_count_24h & 0xFF);
    p[8]  = (uint8_t)(swing_count_24h >> 8);
    p[9]  = (uint8_t)imu_temp;
    p[10] = haptic_last;
    p[11] = haptic_active;
    p[12] = (uint8_t)cane_tilt_deg;
    p[13] = (uint8_t)ble_rssi;
    msg->payload_len = 14;

    return 0;
}

/* Build haptic band telemetry payload (12 bytes) */
int gs_build_band_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, int8_t imu_temp,
                        uint16_t step_count_24h, uint8_t fall_count_24h,
                        uint8_t haptic_last, uint8_t nav_direction,
                        uint8_t nav_distance_m, uint8_t sos_armed,
                        int8_t ble_rssi, uint16_t uptime_min)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = GS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = GS_TELEM_BAND;
    p[1]  = battery_v;
    p[2]  = (uint8_t)imu_temp;
    p[3]  = (uint8_t)(step_count_24h & 0xFF);
    p[4]  = (uint8_t)(step_count_24h >> 8);
    p[5]  = fall_count_24h;
    p[6]  = haptic_last;
    p[7]  = nav_direction;
    p[8]  = nav_distance_m;
    p[9]  = sos_armed;
    p[10] = (uint8_t)ble_rssi;
    p[11] = (uint8_t)(uptime_min & 0xFF);
    /* uptime high byte packed in 12 */
    p[12] = (uint8_t)(uptime_min >> 8);
    msg->payload_len = 13;

    return 0;
}

/* Build command message */
int gs_build_command(gs_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len)
{
    if (!msg || (cmd_len > 0 && !cmd_data)) return -1;
    if (cmd_len > GS_MAX_PAYLOAD - 1) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = GS_MSG_COMMAND;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = cmd_type;
    if (cmd_len > 0)
        memcpy(&msg->payload[1], cmd_data, cmd_len);
    msg->payload_len = 1 + cmd_len;

    return 0;
}

/* Build alert message */
int gs_build_alert(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len)
{
    if (!msg) return -1;
    if (data_len > GS_MAX_PAYLOAD - 2) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = GS_MSG_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = alert_type;
    msg->payload[1] = severity;
    if (data_len > 0)
        memcpy(&msg->payload[2], alert_data, data_len);
    msg->payload_len = 2 + data_len;

    return 0;
}

/* Build navigation update (6 bytes payload) */
int gs_build_nav_update(gs_message_t *msg, uint8_t src, uint8_t dst,
                        uint16_t msg_seq, uint8_t direction,
                        uint8_t distance_m, uint16_t landmark_id,
                        uint8_t eta_min, uint8_t step_index)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = GS_MSG_NAV_UPDATE;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = direction;
    msg->payload[1] = distance_m;
    msg->payload[2] = (uint8_t)(landmark_id & 0xFF);
    msg->payload[3] = (uint8_t)(landmark_id >> 8);
    msg->payload[4] = eta_min;
    msg->payload[5] = step_index;
    msg->payload_len = 6;

    return 0;
}

/* Build scene description (variable payload) */
int gs_build_scene_desc(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t obj_count, const uint8_t *obj_classes,
                        const uint8_t *obj_distances_dm, const int8_t *obj_directions,
                        uint8_t crosswalk_state, uint8_t text_len, const char *text)
{
    if (!msg) return -1;
    if (obj_count > 10) obj_count = 10; /* Cap at 10 objects */

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = GS_MSG_SCENE_DESC;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = obj_count;
    p[1] = crosswalk_state;

    uint8_t idx = 2;
    for (uint8_t i = 0; i < obj_count && idx < GS_MAX_PAYLOAD - 4; i++) {
        p[idx++] = obj_classes[i];
        p[idx++] = obj_distances_dm[i];
        p[idx++] = (uint8_t)obj_directions[i]; /* -90 to +90 degrees */
    }

    /* Append text (if any, from OCR) */
    if (text_len > 0 && text && idx + text_len + 1 < GS_MAX_PAYLOAD) {
        p[idx++] = text_len;
        memcpy(&p[idx], text, text_len);
        idx += text_len;
    } else {
        p[idx++] = 0; /* No text */
    }

    msg->payload_len = idx;
    return 0;
}

/* Build fall alert (8 bytes payload) */
int gs_build_fall_alert(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t impact_mg, int8_t temp_c, uint16_t step_count,
                        uint8_t battery_v, uint8_t stillness_sec)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00; /* Hub */
    msg->header.type = GS_MSG_FALL_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = impact_mg;
    msg->payload[1] = (uint8_t)temp_c;
    msg->payload[2] = (uint8_t)(step_count & 0xFF);
    msg->payload[3] = (uint8_t)(step_count >> 8);
    msg->payload[4] = battery_v;
    msg->payload[5] = stillness_sec;
    msg->payload[6] = 0; /* reserved */
    msg->payload[7] = 0; /* reserved */
    msg->payload_len = 8;

    return 0;
}

/* Build SOS alert (4 bytes payload) */
int gs_build_sos_alert(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                       uint8_t battery_v, uint8_t press_duration_s)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = GS_MSG_SOS_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = battery_v;
    msg->payload[1] = press_duration_s;
    msg->payload[2] = 0; /* reserved */
    msg->payload[3] = 0; /* reserved */
    msg->payload_len = 4;

    return 0;
}

/* Build beacon scan result (variable, 4 + 5×N) */
int gs_build_beacon_scan(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                         uint8_t beacon_count, const uint16_t *beacon_uuids,
                         const int8_t *rssi_vals, const uint8_t *dist_dm)
{
    if (!msg || !beacon_uuids || !rssi_vals || !dist_dm) return -1;
    if (beacon_count > 32) beacon_count = 32;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = GS_MSG_BEACON_SCAN;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = beacon_count;
    p[1] = 0; /* reserved */
    p[2] = 0; /* reserved */
    p[3] = 0; /* reserved */

    uint8_t idx = 4;
    for (uint8_t i = 0; i < beacon_count && idx + 5 <= GS_MAX_PAYLOAD; i++) {
        p[idx++] = (uint8_t)(beacon_uuids[i] & 0xFF);
        p[idx++] = (uint8_t)(beacon_uuids[i] >> 8);
        p[idx++] = (uint8_t)rssi_vals[i];
        p[idx++] = dist_dm[i];
        p[idx++] = 0; /* reserved per-beacon */
    }

    msg->payload_len = idx;
    return 0;
}