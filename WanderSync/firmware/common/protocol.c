/*
 * WanderSync — Protocol Implementation
 * Binary message encoding/decoding with CRC-16-CCITT for Sub-GHz TDMA mesh
 */
#include "protocol.h"

/* === CRC-16-CCITT (0x1021, init 0xFFFF) === */
uint16_t ws_crc16_ccitt(const uint8_t *data, size_t len)
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

/* === Encode message into buffer === */
size_t ws_encode(const ws_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (!msg || !buf) return 0;
    size_t total = WS_HEADER_LEN + msg->payload_len + WS_CRC_LEN;
    if (total > buf_len || msg->payload_len > WS_MAX_PAYLOAD) return 0;

    size_t idx = 0;
    buf[idx++] = WS_SYNC0;
    buf[idx++] = WS_SYNC1;
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);

    if (msg->payload_len > 0)
        memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    uint16_t crc = ws_crc16_ccitt(&buf[2], idx - 2);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}

/* === Decode buffer into message === */
int ws_decode(ws_message_t *msg, const uint8_t *buf, size_t len)
{
    if (!msg || !buf || len < (WS_HEADER_LEN + WS_CRC_LEN)) return -1;
    if (buf[0] != WS_SYNC0 || buf[1] != WS_SYNC1) return -1;

    uint16_t recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    size_t crc_data_len = len - 2 - WS_CRC_LEN;
    uint16_t calc_crc = ws_crc16_ccitt(&buf[2], crc_data_len);
    if (recv_crc != calc_crc) return -2;

    msg->header.src      = buf[2];
    msg->header.dst      = buf[3];
    msg->header.type     = buf[4];
    msg->header.msg_id   = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);

    msg->payload_len = (uint8_t)(len - WS_HEADER_LEN - WS_CRC_LEN);
    if (msg->payload_len > WS_MAX_PAYLOAD) return -1;

    if (msg->payload_len > 0)
        memcpy(msg->payload, &buf[7], msg->payload_len);

    return 0;
}

/* === Build Wander/Fall/SOS Alert (16 bytes) === */
int ws_build_wander_alert(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                          uint8_t alert_type, int32_t lat_e7, int32_t lon_e7,
                          uint8_t wander_risk, uint8_t activity,
                          uint8_t geofence_status, uint8_t battery_v,
                          uint8_t impact_g, uint8_t on_wrist, uint8_t hr)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = WS_BROADCAST;
    msg->header.type = WS_MSG_WANDER_ALERT;
    msg->header.msg_id = msg_id;

    ws_wander_alert_t *wa = (ws_wander_alert_t *)msg->payload;
    wa->alert_type      = alert_type;
    wa->gps_lat_e7      = lat_e7;
    wa->gps_lon_e7      = lon_e7;
    wa->wander_risk     = wander_risk;
    wa->activity_class  = activity;
    wa->geofence_status = geofence_status;
    wa->battery_v       = battery_v;
    wa->impact_g_x10    = impact_g;
    wa->band_on_wrist   = on_wrist;
    wa->hr_bpm          = hr;

    msg->payload_len = sizeof(ws_wander_alert_t);
    return 0;
}

/* === Build Door Lock Command (6 bytes) === */
int ws_build_door_lock_cmd(ws_message_t *msg, uint8_t src, uint8_t dst,
                           uint16_t msg_id, uint8_t door_id, uint8_t action,
                           uint8_t schedule_id, uint16_t duration_s,
                           uint8_t priority)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = WS_MSG_DOOR_LOCK_CMD;
    msg->header.msg_id = msg_id;

    ws_door_lock_cmd_t *cmd = (ws_door_lock_cmd_t *)msg->payload;
    cmd->door_id     = door_id;
    cmd->action      = action;
    cmd->schedule_id = schedule_id;
    cmd->duration_s  = duration_s;
    cmd->priority    = priority;

    msg->payload_len = sizeof(ws_door_lock_cmd_t);
    return 0;
}

/* === Build Reminder Trigger (8 bytes) === */
int ws_build_reminder_trigger(ws_message_t *msg, uint8_t src, uint8_t dst,
                              uint16_t msg_id, uint8_t reminder_id,
                              uint8_t clip_index, uint8_t volume,
                              uint8_t repeat_count, uint8_t repeat_delay,
                              uint8_t tone_before, uint8_t reminder_type,
                              uint8_t ack_timeout)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = WS_MSG_REMINDER_TRIG;
    msg->header.msg_id = msg_id;

    ws_reminder_trigger_t *rt = (ws_reminder_trigger_t *)msg->payload;
    rt->reminder_id  = reminder_id;
    rt->clip_index   = clip_index;
    rt->volume       = volume;
    rt->repeat_count = repeat_count;
    rt->repeat_delay = repeat_delay;
    rt->tone_before  = tone_before;
    rt->reminder_type= reminder_type;
    rt->ack_timeout  = ack_timeout;

    msg->payload_len = sizeof(ws_reminder_trigger_t);
    return 0;
}

/* === Build Band telemetry === */
int ws_build_band_telem(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                        const ws_band_telem_t *telem)
{
    if (!msg || !telem) return -1;
    msg->header.src = src;
    msg->header.dst = WS_HUB_NODE_ID;
    msg->header.type = WS_MSG_TELEMETRY;
    msg->header.msg_id = msg_id;
    memcpy(msg->payload, telem, sizeof(ws_band_telem_t));
    msg->payload_len = sizeof(ws_band_telem_t);
    return 0;
}

/* === Build Door telemetry === */
int ws_build_door_telem(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                        const ws_door_telem_t *telem)
{
    if (!msg || !telem) return -1;
    msg->header.src = src;
    msg->header.dst = WS_HUB_NODE_ID;
    msg->header.type = WS_MSG_TELEMETRY;
    msg->header.msg_id = msg_id;
    memcpy(msg->payload, telem, sizeof(ws_door_telem_t));
    msg->payload_len = sizeof(ws_door_telem_t);
    return 0;
}

/* === Build Room telemetry === */
int ws_build_room_telem(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                        const ws_room_telem_t *telem)
{
    if (!msg || !telem) return -1;
    msg->header.src = src;
    msg->header.dst = WS_HUB_NODE_ID;
    msg->header.type = WS_MSG_TELEMETRY;
    msg->header.msg_id = msg_id;
    memcpy(msg->payload, telem, sizeof(ws_room_telem_t));
    msg->payload_len = sizeof(ws_room_telem_t);
    return 0;
}

/* === Build Voice telemetry === */
int ws_build_voice_telem(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                         const ws_voice_telem_t *telem)
{
    if (!msg || !telem) return -1;
    msg->header.src = src;
    msg->header.dst = WS_HUB_NODE_ID;
    msg->header.type = WS_MSG_TELEMETRY;
    msg->header.msg_id = msg_id;
    memcpy(msg->payload, telem, sizeof(ws_voice_telem_t));
    msg->payload_len = sizeof(ws_voice_telem_t);
    return 0;
}

/* === Build Geofence Update === */
int ws_build_geofence_update(ws_message_t *msg, uint8_t src, uint8_t dst,
                             uint16_t msg_id, const ws_geofence_t *gf)
{
    if (!msg || !gf) return -1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = WS_MSG_GEOFENCE_UPD;
    msg->header.msg_id = msg_id;
    memcpy(msg->payload, gf, sizeof(ws_geofence_t));
    msg->payload_len = sizeof(ws_geofence_t);
    return 0;
}

/* === Build Heartbeat === */
int ws_build_heartbeat(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                       uint8_t battery_v, int8_t rssi, uint16_t uptime_min)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = WS_HUB_NODE_ID;
    msg->header.type = WS_MSG_HEARTBEAT;
    msg->header.msg_id = msg_id;

    msg->payload[0] = battery_v;
    msg->payload[1] = (uint8_t)rssi;
    msg->payload[2] = (uint8_t)(uptime_min & 0xFF);
    msg->payload[3] = (uint8_t)(uptime_min >> 8);
    msg->payload_len = 4;
    return 0;
}

/* === Build Command === */
int ws_build_command(ws_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_id, uint8_t cmd_type, const uint8_t *params,
                     uint8_t param_len)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = WS_MSG_COMMAND;
    msg->header.msg_id = msg_id;

    msg->payload[0] = cmd_type;
    if (params && param_len > 0 && param_len < WS_MAX_PAYLOAD - 1) {
        memcpy(&msg->payload[1], params, param_len);
        msg->payload_len = 1 + param_len;
    } else {
        msg->payload_len = 1;
    }
    return 0;
}

/* === Build Join Request === */
int ws_build_join_req(ws_message_t *msg, uint8_t src, uint16_t msg_id,
                      uint8_t node_type, uint8_t battery_v,
                      uint8_t fw_major, uint8_t fw_minor)
{
    if (!msg) return -1;
    msg->header.src = src;
    msg->header.dst = WS_HUB_NODE_ID;
    msg->header.type = WS_MSG_JOIN_REQ;
    msg->header.msg_id = msg_id;

    msg->payload[0] = node_type;
    msg->payload[1] = battery_v;
    msg->payload[2] = fw_major;
    msg->payload[3] = fw_minor;
    msg->payload_len = 4;
    return 0;
}