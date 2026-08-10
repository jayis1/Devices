/*
 * FireSync — Protocol Implementation
 * Binary message encoding/decoding with CRC-16-CCITT for Sub-GHz TDMA mesh
 *
 * Sub-GHz is a shared medium (no link-layer encryption/CRC like BLE),
 * so application-layer CRC + AES-128-CTR encryption are essential.
 */
#include "protocol.h"

/* === CRC-16-CCITT (0x1021, init 0xFFFF) === */
uint16_t fs_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

/* === Encode message into buffer (header + payload + CRC) === */
size_t fs_encode(const fs_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (!msg || !buf) return 0;
    size_t total = FS_HEADER_LEN + msg->payload_len + FS_CRC_LEN;
    if (total > buf_len || msg->payload_len > FS_MAX_PAYLOAD) return 0;

    size_t idx = 0;
    /* Header */
    buf[idx++] = FS_SYNC0;
    buf[idx++] = FS_SYNC1;
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);

    /* Payload */
    if (msg->payload_len > 0)
        memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    /* CRC over header (excluding sync) + payload */
    uint16_t crc = fs_crc16_ccitt(&buf[2], idx - 2);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}

/* === Decode buffer into message === */
int fs_decode(fs_message_t *msg, const uint8_t *buf, size_t len)
{
    if (!msg || !buf || len < (FS_HEADER_LEN + FS_CRC_LEN)) return -1;

    if (buf[0] != FS_SYNC0 || buf[1] != FS_SYNC1) return -1;

    /* Verify CRC */
    uint16_t recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    uint16_t calc_crc = fs_crc16_ccitt(&buf[2], len - 2 - FS_CRC_LEN + FS_HEADER_LEN - 2);
    /* CRC covers bytes [2..len-3] = src..payload_end */
    size_t crc_data_len = len - 2 /* sync */ - FS_CRC_LEN;
    calc_crc = fs_crc16_ccitt(&buf[2], crc_data_len);
    if (recv_crc != calc_crc) return -2; /* CRC mismatch */

    msg->header.src      = buf[2];
    msg->header.dst      = buf[3];
    msg->header.type     = buf[4];
    msg->header.msg_id   = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);

    msg->payload_len = (uint8_t)(len - FS_HEADER_LEN - FS_CRC_LEN);
    if (msg->payload_len > FS_MAX_PAYLOAD) return -1;

    if (msg->payload_len > 0)
        memcpy(msg->payload, &buf[7], msg->payload_len);

    return 0;
}

/* === Build Fire Alert payload (12 bytes) === */
int fs_build_fire_alert(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                        uint8_t fire_class, uint8_t confidence, uint8_t room_id,
                        uint16_t smoke_pm25, uint16_t co_ppm,
                        int16_t temp_c_x10, int16_t thermal_max_c_x10,
                        uint8_t occupant)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = FS_BROADCAST;
    msg->header.type = FS_MSG_FIRE_ALERT;
    msg->header.msg_id = msg_id;

    fs_fire_alert_t *fa = (fs_fire_alert_t *)msg->payload;
    fa->fire_class       = fire_class;
    fa->confidence       = confidence;
    fa->room_id          = room_id;
    fa->smoke_pm25       = smoke_pm25;
    fa->co_ppm           = co_ppm;
    fa->temp_c_x10       = temp_c_x10;
    fa->thermal_max_c_x10= thermal_max_c_x10;
    fa->occupant         = occupant;

    msg->payload_len = sizeof(fs_fire_alert_t);
    return 0;
}

/* === Build Escape Route payload (8 bytes) === */
int fs_build_escape_route(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                          uint8_t fire_room, uint8_t safe_exit,
                          uint16_t avoid_rooms, uint8_t led_path_mask,
                          uint8_t led_red_mask, uint8_t voice_msg_id,
                          uint8_t door_mask)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = FS_BROADCAST;
    msg->header.type = FS_MSG_ESCAPE_UPDATE;
    msg->header.msg_id = msg_id;

    fs_escape_route_t *er = (fs_escape_route_t *)msg->payload;
    er->fire_room     = fire_room;
    er->safe_exit     = safe_exit;
    er->avoid_rooms   = avoid_rooms;
    er->led_path_mask = led_path_mask;
    er->led_red_mask  = led_red_mask;
    er->voice_msg_id  = voice_msg_id;
    er->door_mask     = door_mask;

    msg->payload_len = sizeof(fs_escape_route_t);
    return 0;
}

/* === Build Sentinel telemetry === */
int fs_build_sentinel_telem(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                            const fs_sentinel_telem_t *telem)
{
    if (!msg || !telem) return -1;
    msg->header.src = src;
    msg->header.dst = FS_HUB_NODE_ID;
    msg->header.type = FS_MSG_TELEMETRY;
    msg->header.msg_id = msg_id;

    memcpy(msg->payload, telem, sizeof(fs_sentinel_telem_t));
    msg->payload_len = sizeof(fs_sentinel_telem_t);
    return 0;
}

/* === Build Stove telemetry === */
int fs_build_stove_telem(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                         const fs_stove_telem_t *telem)
{
    if (!msg || !telem) return -1;
    msg->header.src = src;
    msg->header.dst = FS_HUB_NODE_ID;
    msg->header.type = FS_MSG_TELEMETRY;
    msg->header.msg_id = msg_id;

    memcpy(msg->payload, telem, sizeof(fs_stove_telem_t));
    msg->payload_len = sizeof(fs_stove_telem_t);
    return 0;
}

/* === Build Panel telemetry === */
int fs_build_panel_telem(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                         const fs_panel_telem_t *telem)
{
    if (!msg || !telem) return -1;
    msg->header.src = src;
    msg->header.dst = FS_HUB_NODE_ID;
    msg->header.type = FS_MSG_TELEMETRY;
    msg->header.msg_id = msg_id;

    memcpy(msg->payload, telem, sizeof(fs_panel_telem_t));
    msg->payload_len = sizeof(fs_panel_telem_t);
    return 0;
}

/* === Build Escape telemetry === */
int fs_build_escape_telem(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                          const fs_escape_telem_t *telem)
{
    if (!msg || !telem) return -1;
    msg->header.src = src;
    msg->header.dst = FS_HUB_NODE_ID;
    msg->header.type = FS_MSG_TELEMETRY;
    msg->header.msg_id = msg_id;

    memcpy(msg->payload, telem, sizeof(fs_escape_telem_t));
    msg->payload_len = sizeof(fs_escape_telem_t);
    return 0;
}

/* === Build Heartbeat === */
int fs_build_heartbeat(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                       uint8_t battery_v, int8_t rssi, uint16_t uptime_min)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = FS_HUB_NODE_ID;
    msg->header.type = FS_MSG_HEARTBEAT;
    msg->header.msg_id = msg_id;

    msg->payload[0] = battery_v;
    msg->payload[1] = (uint8_t)rssi;
    msg->payload[2] = (uint8_t)(uptime_min & 0xFF);
    msg->payload[3] = (uint8_t)(uptime_min >> 8);
    msg->payload_len = 4;
    return 0;
}

/* === Build Command === */
int fs_build_command(fs_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_id, uint8_t cmd_type, const uint8_t *params,
                     uint8_t param_len)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = FS_MSG_COMMAND;
    msg->header.msg_id = msg_id;

    msg->payload[0] = cmd_type;
    if (params && param_len > 0 && param_len < FS_MAX_PAYLOAD - 1) {
        memcpy(&msg->payload[1], params, param_len);
        msg->payload_len = 1 + param_len;
    } else {
        msg->payload_len = 1;
    }
    return 0;
}

/* === Build Join Request === */
int fs_build_join_req(fs_message_t *msg, uint8_t src, uint16_t msg_id,
                      uint8_t node_type, uint8_t battery_v,
                      uint8_t fw_major, uint8_t fw_minor)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = FS_HUB_NODE_ID;
    msg->header.type = FS_MSG_JOIN_REQ;
    msg->header.msg_id = msg_id;

    msg->payload[0] = node_type;
    msg->payload[1] = battery_v;
    msg->payload[2] = fw_major;
    msg->payload[3] = fw_minor;
    msg->payload_len = 4;
    return 0;
}