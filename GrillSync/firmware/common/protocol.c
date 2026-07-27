/*
 * GrillSync — Protocol Implementation
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#include "protocol.h"

/* CRC-16-CCITT (0x1021 polynomial, init 0xFFFF) */
uint16_t gs_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ GS_CRC_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* Encode message into buffer, returns total length (0 on error) */
size_t gs_encode(const gs_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (buf_len < 9 + msg->payload_len + 2)
        return 0;

    size_t idx = 0;
    buf[idx++] = msg->header.sync[0];
    buf[idx++] = msg->header.sync[1];
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);
    buf[idx++] = msg->payload_len;

    if (msg->payload_len > 0)
        memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    uint16_t crc = gs_crc16(buf, idx);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}

/* Decode buffer into message, returns 0 on success, -1 on error */
int gs_decode(gs_message_t *msg, const uint8_t *buf, size_t len)
{
    if (len < 11)
        return -1;

    msg->header.sync[0] = buf[0];
    msg->header.sync[1] = buf[1];

    if (msg->header.sync[0] != GS_SYNC0 || msg->header.sync[1] != GS_SYNC1)
        return -1;

    msg->header.src = buf[2];
    msg->header.dst = buf[3];
    msg->header.type = buf[4];
    msg->header.msg_id = buf[5] | (buf[6] << 8);
    msg->payload_len = buf[7];

    if (len < 9 + msg->payload_len + 2)
        return -1;

    if (msg->payload_len > 0)
        memcpy(msg->payload, &buf[8], msg->payload_len);

    uint16_t crc_calc = gs_crc16(buf, 8 + msg->payload_len);
    uint16_t crc_recv = buf[8 + msg->payload_len] | (buf[9 + msg->payload_len] << 8);
    if (crc_calc != crc_recv)
        return -1;

    msg->crc = crc_recv;
    return 0;
}

/* Build grill sentinel telemetry (24 bytes payload) */
int gs_build_sentinel_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                             uint8_t battery_v, int16_t surface_max_deci,
                             int16_t surface_avg_deci, uint8_t hot_zone_count,
                             uint16_t gas_ppm, uint8_t gas_lel_pct,
                             uint8_t flame_intensity, uint8_t flame_detected,
                             int16_t ambient_temp_deci, uint16_t humidity_deci,
                             uint16_t acoustic_energy, uint8_t flareup_risk,
                             uint16_t flareup_eta_100ms, uint16_t event_id,
                             int8_t rssi)
{
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = GS_HUB_NODE_ID;
    msg->header.type = GS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = GS_TELEM_SENTINEL;
    p[1]  = battery_v;
    p[2]  = (uint8_t)(surface_max_deci & 0xFF);
    p[3]  = (uint8_t)(surface_max_deci >> 8);
    p[4]  = (uint8_t)(surface_avg_deci & 0xFF);
    p[5]  = (uint8_t)(surface_avg_deci >> 8);
    p[6]  = hot_zone_count;
    p[7]  = (uint8_t)(gas_ppm & 0xFF);
    p[8]  = (uint8_t)(gas_ppm >> 8);
    p[9]  = gas_lel_pct;
    p[10] = flame_intensity;
    p[11] = flame_detected;
    p[12] = (uint8_t)(ambient_temp_deci & 0xFF);
    p[13] = (uint8_t)(ambient_temp_deci >> 8);
    p[14] = (uint8_t)(humidity_deci & 0xFF);
    p[15] = (uint8_t)(humidity_deci >> 8);
    p[16] = (uint8_t)(acoustic_energy & 0xFF);
    p[17] = (uint8_t)(acoustic_energy >> 8);
    p[18] = flareup_risk;
    p[19] = (uint8_t)(flareup_eta_100ms & 0xFF);
    p[20] = (uint8_t)(flareup_eta_100ms >> 8);
    p[21] = (uint8_t)(event_id & 0xFF);
    p[22] = (uint8_t)(event_id >> 8);
    p[23] = (uint8_t)rssi;

    msg->payload_len = 24;
    return 0;
}

/* Build meat probe telemetry (18 bytes payload) */
int gs_build_probe_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                          uint8_t battery_v, uint8_t probe_id, uint8_t meat_type,
                          int16_t temp_tip_deci, int16_t temp_mid_deci,
                          int16_t temp_surface_deci, int16_t temp_ambient_deci,
                          int16_t target_temp_deci, uint8_t doneness_level,
                          uint16_t doneness_eta_10s, int8_t rssi)
{
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = GS_HUB_NODE_ID;
    msg->header.type = GS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = GS_TELEM_PROBE;
    p[1]  = battery_v;
    p[2]  = probe_id;
    p[3]  = meat_type;
    p[4]  = (uint8_t)(temp_tip_deci & 0xFF);
    p[5]  = (uint8_t)(temp_tip_deci >> 8);
    p[6]  = (uint8_t)(temp_mid_deci & 0xFF);
    p[7]  = (uint8_t)(temp_mid_deci >> 8);
    p[8]  = (uint8_t)(temp_surface_deci & 0xFF);
    p[9]  = (uint8_t)(temp_surface_deci >> 8);
    p[10] = (uint8_t)(temp_ambient_deci & 0xFF);
    p[11] = (uint8_t)(temp_ambient_deci >> 8);
    p[12] = (uint8_t)(target_temp_deci & 0xFF);
    p[13] = (uint8_t)(target_temp_deci >> 8);
    p[14] = doneness_level;
    p[15] = (uint8_t)(doneness_eta_10s & 0xFF);
    p[16] = (uint8_t)(doneness_eta_10s >> 8);
    p[17] = (uint8_t)rssi;

    msg->payload_len = 18;
    return 0;
}

/* Build smoke node telemetry (22 bytes payload) */
int gs_build_smoke_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                         uint8_t battery_v, uint16_t pm1_0_deci, uint16_t pm2_5_deci,
                         uint16_t pm10_deci, uint16_t voc_index,
                         uint16_t gas_resistance_100ohm, uint16_t co2eq_ppm,
                         uint8_t smoke_quality, uint8_t flame_intensity,
                         int16_t temp_deci, uint16_t humidity_deci, int8_t rssi)
{
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = GS_HUB_NODE_ID;
    msg->header.type = GS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = GS_TELEM_SMOKE;
    p[1]  = battery_v;
    p[2]  = (uint8_t)(pm1_0_deci & 0xFF);
    p[3]  = (uint8_t)(pm1_0_deci >> 8);
    p[4]  = (uint8_t)(pm2_5_deci & 0xFF);
    p[5]  = (uint8_t)(pm2_5_deci >> 8);
    p[6]  = (uint8_t)(pm10_deci & 0xFF);
    p[7]  = (uint8_t)(pm10_deci >> 8);
    p[8]  = (uint8_t)(voc_index & 0xFF);
    p[9]  = (uint8_t)(voc_index >> 8);
    p[10] = (uint8_t)(gas_resistance_100ohm & 0xFF);
    p[11] = (uint8_t)(gas_resistance_100ohm >> 8);
    p[12] = (uint8_t)(co2eq_ppm & 0xFF);
    p[13] = (uint8_t)(co2eq_ppm >> 8);
    p[14] = smoke_quality;
    p[15] = flame_intensity;
    p[16] = (uint8_t)(temp_deci & 0xFF);
    p[17] = (uint8_t)(temp_deci >> 8);
    p[18] = (uint8_t)(humidity_deci & 0xFF);
    p[19] = (uint8_t)(humidity_deci >> 8);
    p[20] = (uint8_t)rssi;
    /* padding byte */
    p[21] = 0;

    msg->payload_len = 22;
    return 0;
}

/* Build doneness update broadcast */
int gs_build_doneness_update(gs_message_t *msg, uint8_t src, uint16_t msg_seq,
                              uint8_t probe_id, uint8_t meat_type,
                              uint8_t doneness_level, uint16_t eta_10s,
                              int16_t current_temp_deci, int16_t target_temp_deci)
{
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = src;
    msg->header.dst = GS_BROADCAST;
    msg->header.type = GS_MSG_DONENESS_UPDATE;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = probe_id;
    p[1] = meat_type;
    p[2] = doneness_level;
    p[3] = (uint8_t)(eta_10s & 0xFF);
    p[4] = (uint8_t)(eta_10s >> 8);
    p[5] = (uint8_t)(current_temp_deci & 0xFF);
    p[6] = (uint8_t)(current_temp_deci >> 8);
    p[7] = (uint8_t)(target_temp_deci & 0xFF);
    p[8] = (uint8_t)(target_temp_deci >> 8);

    msg->payload_len = 9;
    return 0;
}

/* Build a command message */
int gs_build_command(gs_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len)
{
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = GS_MSG_COMMAND;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = cmd_type;
    if (cmd_len > 0 && cmd_len <= GS_MAX_PAYLOAD - 1) {
        memcpy(&msg->payload[1], cmd_data, cmd_len);
        msg->payload_len = 1 + cmd_len;
    } else {
        msg->payload_len = 1;
    }
    return 0;
}

/* Build an alert message */
int gs_build_alert(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len)
{
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = GS_HUB_NODE_ID;
    msg->header.type = GS_MSG_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = alert_type;
    msg->payload[1] = severity;
    if (data_len > 0 && data_len <= GS_MAX_PAYLOAD - 2) {
        memcpy(&msg->payload[2], alert_data, data_len);
        msg->payload_len = 2 + data_len;
    } else {
        msg->payload_len = 2;
    }
    return 0;
}

/* Build a thermal frame message */
int gs_build_thermal_frame(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                            uint8_t frame_seq, const uint8_t *compressed,
                            uint8_t compressed_len, int16_t max_deci,
                            int16_t avg_deci, uint8_t hot_zones)
{
    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = GS_SYNC0;
    msg->header.sync[1] = GS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = GS_HUB_NODE_ID;
    msg->header.type = GS_MSG_THERMAL_FRAME;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = frame_seq;
    p[1] = (uint8_t)(max_deci & 0xFF);
    p[2] = (uint8_t)(max_deci >> 8);
    p[3] = (uint8_t)(avg_deci & 0xFF);
    p[4] = (uint8_t)(avg_deci >> 8);
    p[5] = hot_zones;

    if (compressed_len > GS_MAX_PAYLOAD - 6)
        compressed_len = GS_MAX_PAYLOAD - 6;
    memcpy(&p[6], compressed, compressed_len);
    msg->payload_len = 6 + compressed_len;
    return 0;
}