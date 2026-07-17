/*
 * StormSync — Protocol Implementation
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#include "protocol.h"

/* CRC-16-CCITT (0xFFFF init, poly 0x1021) */
uint16_t ss_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ SS_CRC_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* Encode message into buffer */
size_t ss_encode(const ss_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (!msg || !buf) return 0;
    size_t total = 7 + msg->payload_len + 2; /* header(7) + payload + CRC */
    if (total > buf_len || msg->payload_len > SS_MAX_PAYLOAD) return 0;

    size_t idx = 0;
    /* Header */
    buf[idx++] = SS_SYNC0;
    buf[idx++] = SS_SYNC1;
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);

    /* Payload */
    memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    /* CRC over all preceding bytes */
    uint16_t crc = ss_crc16(buf, idx);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}

/* Decode buffer into message */
int ss_decode(ss_message_t *msg, const uint8_t *buf, size_t len)
{
    if (!msg || !buf || len < 9) return -1; /* min: 7 header + 2 CRC */

    if (buf[0] != SS_SYNC0 || buf[1] != SS_SYNC1) return -1;

    msg->header.src      = buf[2];
    msg->header.dst      = buf[3];
    msg->header.type     = buf[4];
    msg->header.msg_id   = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);

    msg->payload_len = (uint8_t)(len - 9);
    if (msg->payload_len > SS_MAX_PAYLOAD) return -1;

    memcpy(msg->payload, &buf[7], msg->payload_len);

    uint16_t calc_crc = ss_crc16(buf, len - 2);
    uint16_t recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    if (calc_crc != recv_crc) return -1;

    msg->crc = recv_crc;
    return 0;
}

/* Build sump sentinel telemetry payload (19 bytes) */
int ss_build_sump_telem(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, uint16_t water_level_mm,
                        uint16_t pump_current, uint8_t pump_status,
                        uint16_t flow_rate, int16_t water_temp_deci,
                        uint16_t vib_rms, uint16_t vib_peak,
                        uint8_t mains_ok, uint16_t pump_runtime_min,
                        int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = SS_SYNC0;
    msg->header.sync[1] = SS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = SS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = SS_TELEM_SUMP;
    p[1]  = battery_v;
    p[2]  = (uint8_t)(water_level_mm & 0xFF);
    p[3]  = (uint8_t)(water_level_mm >> 8);
    p[4]  = (uint8_t)(pump_current & 0xFF);
    p[5]  = (uint8_t)(pump_current >> 8);
    p[6]  = pump_status;
    p[7]  = (uint8_t)(flow_rate & 0xFF);
    p[8]  = (uint8_t)(flow_rate >> 8);
    p[9]  = (uint8_t)(water_temp_deci & 0xFF);
    p[10] = (uint8_t)(water_temp_deci >> 8);
    p[11] = (uint8_t)(vib_rms & 0xFF);
    p[12] = (uint8_t)(vib_rms >> 8);
    p[13] = (uint8_t)(vib_peak & 0xFF);
    p[14] = (uint8_t)(vib_peak >> 8);
    p[15] = mains_ok;
    p[16] = (uint8_t)(pump_runtime_min & 0xFF);
    p[17] = (uint8_t)(pump_runtime_min >> 8);
    p[18] = (uint8_t)rssi;
    msg->payload_len = 19;

    return 0;
}

/* Build soil saturation telemetry payload (15 bytes) */
int ss_build_soil_telem(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_mv, uint16_t moist_15, uint16_t moist_45,
                        uint16_t moist_90, int16_t pore_pressure,
                        int8_t temp_15, int8_t temp_45, int8_t temp_90,
                        uint8_t solar_v, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = SS_SYNC0;
    msg->header.sync[1] = SS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = SS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = SS_TELEM_SOIL;
    p[1]  = battery_mv;
    p[2]  = (uint8_t)(moist_15 & 0xFF);
    p[3]  = (uint8_t)(moist_15 >> 8);
    p[4]  = (uint8_t)(moist_45 & 0xFF);
    p[5]  = (uint8_t)(moist_45 >> 8);
    p[6]  = (uint8_t)(moist_90 & 0xFF);
    p[7]  = (uint8_t)(moist_90 >> 8);
    p[8]  = (uint8_t)(pore_pressure & 0xFF);
    p[9]  = (uint8_t)(pore_pressure >> 8);
    p[10] = (uint8_t)temp_15;
    p[11] = (uint8_t)temp_45;
    p[12] = (uint8_t)temp_90;
    p[13] = solar_v;
    p[14] = (uint8_t)rssi;
    msg->payload_len = 15;

    return 0;
}

/* Build weather sentinel telemetry payload (16 bytes) */
int ss_build_weather_telem(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t battery_mv, int16_t temp_deci,
                           uint16_t humidity_deci, uint16_t pressure_deci,
                           uint16_t wind_speed_deci, uint16_t wind_dir,
                           uint16_t rain_tips, uint8_t pressure_trend,
                           int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = SS_SYNC0;
    msg->header.sync[1] = SS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = SS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = SS_TELEM_WEATHER;
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
    p[14] = pressure_trend;
    p[15] = (uint8_t)rssi;
    msg->payload_len = 16;

    return 0;
}

/* Build flood actuator telemetry payload (9 bytes) */
int ss_build_actuator_telem(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                            uint8_t battery_v, uint8_t valve_status,
                            uint8_t pump_relay, uint8_t float_switch,
                            uint8_t mains_ok, uint8_t alarm_status,
                            uint8_t battery_health, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = SS_SYNC0;
    msg->header.sync[1] = SS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = SS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = SS_TELEM_ACTUATOR;
    p[1] = battery_v;
    p[2] = valve_status;
    p[3] = pump_relay;
    p[4] = float_switch;
    p[5] = mains_ok;
    p[6] = alarm_status;
    p[7] = battery_health;
    p[8] = (uint8_t)rssi;
    msg->payload_len = 9;

    return 0;
}

/* Build command message */
int ss_build_command(ss_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len)
{
    if (!msg || (cmd_len > 0 && !cmd_data)) return -1;
    if (cmd_len > SS_MAX_PAYLOAD - 1) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = SS_SYNC0;
    msg->header.sync[1] = SS_SYNC1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = SS_MSG_COMMAND;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = cmd_type;
    if (cmd_len > 0)
        memcpy(&msg->payload[1], cmd_data, cmd_len);
    msg->payload_len = 1 + cmd_len;

    return 0;
}

/* Build alert message */
int ss_build_alert(ss_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len)
{
    if (!msg) return -1;
    if (data_len > SS_MAX_PAYLOAD - 2) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = SS_SYNC0;
    msg->header.sync[1] = SS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = SS_MSG_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = alert_type;
    msg->payload[1] = severity;
    if (data_len > 0)
        memcpy(&msg->payload[2], alert_data, data_len);
    msg->payload_len = 2 + data_len;

    return 0;
}

/* Build flood status broadcast (8 bytes) */
int ss_build_flood_status(ss_message_t *msg, uint8_t src, uint16_t msg_seq,
                          uint8_t risk_level, uint8_t score,
                          uint16_t predicted_level_mm, uint16_t time_to_flood_min)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = SS_SYNC0;
    msg->header.sync[1] = SS_SYNC1;
    msg->header.src = src;
    msg->header.dst = SS_BROADCAST;
    msg->header.type = SS_MSG_FLOOD_STATUS;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = risk_level;   /* 0=low, 1=moderate, 2=high, 3=critical */
    msg->payload[1] = score;        /* 0-100 */
    msg->payload[2] = (uint8_t)(predicted_level_mm & 0xFF);
    msg->payload[3] = (uint8_t)(predicted_level_mm >> 8);
    msg->payload[4] = (uint8_t)(time_to_flood_min & 0xFF);
    msg->payload[5] = (uint8_t)(time_to_flood_min >> 8);
    msg->payload_len = 6;

    return 0;
}