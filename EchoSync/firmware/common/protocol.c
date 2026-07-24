/*
 * EchoSync — Protocol Implementation
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#include "protocol.h"

/* CRC-16-CCITT (XMODEM) */
uint16_t es_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ ES_CRC_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* Encode message into buffer */
size_t es_encode(const es_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (!msg || !buf || buf_len < (size_t)(9 + msg->payload_len + 2))
        return 0;

    size_t idx = 0;
    buf[idx++] = msg->header.sync[0];
    buf[idx++] = msg->header.sync[1];
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);

    memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    /* CRC over header + payload */
    uint16_t crc = es_crc16(buf, idx);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}

/* Decode buffer into message */
int es_decode(es_message_t *msg, const uint8_t *buf, size_t len)
{
    if (!msg || !buf || len < 9)
        return -1;

    if (buf[0] != ES_SYNC0 || buf[1] != ES_SYNC1)
        return -1;

    msg->header.sync[0] = buf[0];
    msg->header.sync[1] = buf[1];
    msg->header.src     = buf[2];
    msg->header.dst     = buf[3];
    msg->header.type    = buf[4];
    msg->header.msg_id  = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);

    msg->payload_len = (uint8_t)(len - 9);
    if (msg->payload_len > ES_MAX_PAYLOAD)
        return -1;
    memcpy(msg->payload, &buf[7], msg->payload_len);

    /* Verify CRC */
    uint16_t expected = es_crc16(buf, len - 2);
    uint16_t received = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    if (expected != received)
        return -1;

    msg->crc = received;
    return 0;
}

/* Build sentinel telemetry (18 bytes) */
int es_build_sentinel_telem(es_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                             uint8_t battery_v, uint8_t sound_class,
                             uint8_t confidence, uint16_t direction_deci,
                             int8_t direction_elev, uint16_t duration_ms,
                             int16_t temp_deci, uint16_t humidity_deci,
                             uint8_t db_spl, uint8_t priority,
                             uint16_t event_id, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = ES_SYNC0;
    msg->header.sync[1] = ES_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00; /* Hub */
    msg->header.type = ES_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = ES_TELEM_SENTINEL;
    p[1] = battery_v;
    p[2] = sound_class;
    p[3] = confidence;
    p[4] = (uint8_t)(direction_deci & 0xFF);
    p[5] = (uint8_t)(direction_deci >> 8);
    p[6] = (uint8_t)direction_elev;
    p[7] = (uint8_t)(duration_ms & 0xFF);
    p[8] = (uint8_t)(duration_ms >> 8);
    p[9] = (uint8_t)(temp_deci & 0xFF);
    p[10] = (uint8_t)(temp_deci >> 8);
    p[11] = (uint8_t)(humidity_deci & 0xFF);
    p[12] = (uint8_t)(humidity_deci >> 8);
    p[13] = db_spl;
    p[14] = priority;
    p[15] = (uint8_t)(event_id & 0xFF);
    p[16] = (uint8_t)(event_id >> 8);
    p[17] = (uint8_t)rssi;
    msg->payload_len = 18;
    return 0;
}

/* Build wrist band telemetry (10 bytes) */
int es_build_wrist_telem(es_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                          uint8_t battery_v, uint8_t worn, uint8_t sleeping,
                          uint8_t last_alert_class, uint8_t last_alert_priority,
                          uint16_t alerts_24h, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = ES_SYNC0;
    msg->header.sync[1] = ES_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = ES_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = ES_TELEM_WRIST;
    p[1] = battery_v;
    p[2] = worn;
    p[3] = sleeping;
    p[4] = last_alert_class;
    p[5] = last_alert_priority;
    p[6] = (uint8_t)(alerts_24h & 0xFF);
    p[7] = (uint8_t)(alerts_24h >> 8);
    p[8] = 0xFF; /* BLE */
    p[9] = 0;    /* reserved */
    msg->payload_len = 10;
    return 0;
}

/* Build door tag telemetry (10 bytes) */
int es_build_door_telem(es_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, uint8_t event_type,
                        uint8_t confidence, uint8_t knock_count,
                        uint16_t event_id, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = ES_SYNC0;
    msg->header.sync[1] = ES_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = ES_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = ES_TELEM_DOOR;
    p[1] = battery_v;
    p[2] = event_type;
    p[3] = confidence;
    p[4] = knock_count;
    p[5] = (uint8_t)(event_id & 0xFF);
    p[6] = (uint8_t)(event_id >> 8);
    p[7] = 0xFF; /* BLE */
    p[8] = 0;
    p[9] = 0;
    msg->payload_len = 10;
    return 0;
}

/* Build sound event broadcast (Hub→Wrist Band, 12 bytes) */
int es_build_sound_event(es_message_t *msg, uint8_t src, uint16_t msg_seq,
                          uint8_t sound_class, uint8_t priority,
                          uint8_t confidence, uint16_t direction_deci,
                          uint8_t source_node, uint16_t room_hash,
                          uint16_t event_id, uint8_t haptic_pattern)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = ES_SYNC0;
    msg->header.sync[1] = ES_SYNC1;
    msg->header.src = src;
    msg->header.dst = ES_BROADCAST;
    msg->header.type = ES_MSG_SOUND_EVENT;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = sound_class;
    p[1] = priority;
    p[2] = confidence;
    p[3] = (uint8_t)(direction_deci & 0xFF);
    p[4] = (uint8_t)(direction_deci >> 8);
    p[5] = source_node;
    p[6] = (uint8_t)(room_hash & 0xFF);
    p[7] = (uint8_t)(room_hash >> 8);
    p[8] = (uint8_t)(event_id & 0xFF);
    p[9] = (uint8_t)(event_id >> 8);
    p[10] = haptic_pattern;
    p[11] = 0; /* reserved */
    msg->payload_len = 12;
    return 0;
}

/* Build a command message */
int es_build_command(es_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len)
{
    if (!msg || cmd_len > (ES_MAX_PAYLOAD - 1)) return -1;
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = ES_SYNC0;
    msg->header.sync[1] = ES_SYNC1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = ES_MSG_COMMAND;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = cmd_type;
    if (cmd_data && cmd_len > 0)
        memcpy(&msg->payload[1], cmd_data, cmd_len);
    msg->payload_len = 1 + cmd_len;
    return 0;
}

/* Build an alert message */
int es_build_alert(es_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len)
{
    if (!msg || data_len > (ES_MAX_PAYLOAD - 2)) return -1;
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = ES_SYNC0;
    msg->header.sync[1] = ES_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00; /* Hub */
    msg->header.type = ES_MSG_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = alert_type;
    msg->payload[1] = severity;
    if (alert_data && data_len > 0)
        memcpy(&msg->payload[2], alert_data, data_len);
    msg->payload_len = 2 + data_len;
    return 0;
}