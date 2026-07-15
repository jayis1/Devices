/*
 * LawnSync — Protocol Implementation
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#include "protocol.h"

/* CRC-16-CCITT (0xFFFF init, poly 0x1021) */
uint16_t ls_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ LS_CRC_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* Encode message into buffer */
size_t ls_encode(const ls_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (!msg || !buf) return 0;
    size_t total = 6 + msg->payload_len + 2; /* header + payload + CRC */
    if (total > buf_len || msg->payload_len > LS_MAX_PAYLOAD) return 0;

    size_t idx = 0;
    /* Header */
    buf[idx++] = LS_SYNC0;
    buf[idx++] = LS_SYNC1;
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);

    /* Payload */
    memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    /* CRC over header + payload (excluding sync bytes? No—include all) */
    uint16_t crc = ls_crc16(buf, idx);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}

/* Decode buffer into message */
int ls_decode(ls_message_t *msg, const uint8_t *buf, size_t len)
{
    if (!msg || !buf || len < 8) return -1; /* min: 6 header + 2 CRC */

    /* Check sync bytes */
    if (buf[0] != LS_SYNC0 || buf[1] != LS_SYNC1) return -1;

    /* Parse header */
    msg->header.src      = buf[2];
    msg->header.dst      = buf[3];
    msg->header.type     = buf[4];
    msg->header.msg_id   = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);

    /* Payload length = total - 6 (header) - 2 (CRC) */
    msg->payload_len = (uint8_t)(len - 8);
    if (msg->payload_len > LS_MAX_PAYLOAD) return -1;

    /* Copy payload */
    memcpy(msg->payload, &buf[7], msg->payload_len);

    /* Verify CRC */
    uint16_t calc_crc = ls_crc16(buf, len - 2);
    uint16_t recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    if (calc_crc != recv_crc) return -1;

    msg->crc = recv_crc;
    return 0;
}

/* Build soil telemetry payload */
int ls_build_soil_telem(ls_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_mv, uint16_t moisture_pct,
                        int16_t temp_deci, uint8_t pH_deci,
                        uint16_t nitrogen, uint16_t phosphorus,
                        uint16_t potassium, uint16_t light_lux,
                        uint8_t solar_v, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = LS_SYNC0;
    msg->header.sync[1] = LS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00; /* Hub */
    msg->header.type = LS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = LS_TELEM_SOIL;
    p[1]  = battery_mv;
    p[2]  = (uint8_t)(moisture_pct & 0xFF);
    p[3]  = (uint8_t)(moisture_pct >> 8);
    p[4]  = (uint8_t)(temp_deci & 0xFF);
    p[5]  = (uint8_t)(temp_deci >> 8);
    p[6]  = pH_deci;
    p[7]  = (uint8_t)(nitrogen & 0xFF);
    p[8]  = (uint8_t)(nitrogen >> 8);
    p[9]  = (uint8_t)(phosphorus & 0xFF);
    p[10] = (uint8_t)(phosphorus >> 8);
    p[11] = (uint8_t)(potassium & 0xFF);
    p[12] = (uint8_t)(potassium >> 8);
    p[13] = (uint8_t)(light_lux & 0xFF);
    p[14] = (uint8_t)(light_lux >> 8);
    p[15] = solar_v;
    p[16] = (uint8_t)rssi;
    p[17] = (uint8_t)(msg_seq & 0xFF);
    p[18] = (uint8_t)(msg_seq >> 8);
    msg->payload_len = 19;

    return 0;
}

/* Build weather telemetry payload */
int ls_build_weather_telem(ls_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                            uint8_t battery_mv, int16_t temp_deci,
                            uint16_t humidity_deci, uint16_t pressure_deci,
                            uint16_t wind_speed_deci, uint16_t wind_dir,
                            uint16_t rain_tips, uint16_t solar_irr,
                            uint8_t uv_index_deci, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = LS_SYNC0;
    msg->header.sync[1] = LS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = LS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = LS_TELEM_WEATHER;
    p[1]  = battery_mv;
    p[2]  = (uint8_t)(temp_deci & 0xFF);
    p[3]  = (uint8_t)(temp_deci >> 8);
    p[4]  = (uint8_t)(humidity_deci & 0xFF);
    p[5]  = (uint8_t)(humidity_deci >> 8);
    p[6]  = (uint8_t)(pressure_deci & 0xFF);
    p[7]  = (uint8_t)(pressure_deci >> 8);
    p[8]  = (uint8_t)(wind_speed_deci & 0xFF);
    p[9]  = (uint8_t)(wind_speed_deci >> 8);
    p[10] = (uint8_t)(wind_dir & 0xFF);
    p[11] = (uint8_t)(wind_dir >> 8);
    p[12] = (uint8_t)(rain_tips & 0xFF);
    p[13] = (uint8_t)(rain_tips >> 8);
    p[14] = (uint8_t)(solar_irr & 0xFF);
    p[15] = (uint8_t)(solar_irr >> 8);
    p[16] = uv_index_deci;
    p[17] = (uint8_t)rssi;
    msg->payload_len = 18;

    return 0;
}

/* Build sprinkler telemetry payload */
int ls_build_sprinkler_telem(ls_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                              uint8_t active_zone, uint16_t flow_rate,
                              uint32_t total_flow, uint16_t pressure,
                              uint8_t rain_detected, uint8_t valve_status,
                              int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = LS_SYNC0;
    msg->header.sync[1] = LS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = LS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = LS_TELEM_SPRINKLER;
    p[1]  = active_zone;
    p[2]  = (uint8_t)(flow_rate & 0xFF);
    p[3]  = (uint8_t)(flow_rate >> 8);
    p[4]  = (uint8_t)(total_flow & 0xFF);
    p[5]  = (uint8_t)((total_flow >> 8) & 0xFF);
    p[6]  = (uint8_t)((total_flow >> 16) & 0xFF);
    p[7]  = (uint8_t)((total_flow >> 24) & 0xFF);
    p[8]  = (uint8_t)(pressure & 0xFF);
    p[9]  = (uint8_t)(pressure >> 8);
    p[10] = rain_detected;
    p[11] = valve_status;
    p[12] = (uint8_t)rssi;
    msg->payload_len = 13;

    return 0;
}

/* Build command message */
int ls_build_command(ls_message_t *msg, uint8_t src, uint8_t dst,
                      uint16_t msg_seq, uint8_t cmd_type,
                      const uint8_t *cmd_data, uint8_t cmd_len)
{
    if (!msg || (cmd_len > 0 && !cmd_data)) return -1;
    if (cmd_len > LS_MAX_PAYLOAD - 1) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = LS_SYNC0;
    msg->header.sync[1] = LS_SYNC1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = LS_MSG_COMMAND;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = cmd_type;
    if (cmd_len > 0)
        memcpy(&msg->payload[1], cmd_data, cmd_len);
    msg->payload_len = 1 + cmd_len;

    return 0;
}

/* Build alert message */
int ls_build_alert(ls_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                    uint8_t alert_type, uint8_t severity,
                    const uint8_t *alert_data, uint8_t data_len)
{
    if (!msg) return -1;
    if (data_len > LS_MAX_PAYLOAD - 2) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = LS_SYNC0;
    msg->header.sync[1] = LS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = LS_MSG_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = alert_type;
    msg->payload[1] = severity;
    if (data_len > 0)
        memcpy(&msg->payload[2], alert_data, data_len);
    msg->payload_len = 2 + data_len;

    return 0;
}