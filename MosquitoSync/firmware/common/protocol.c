/*
 * MosquitoSync — Protocol Implementation
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#include "protocol.h"

/* CRC-16-CCITT (0xFFFF init, poly 0x1021) */
uint16_t ms_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ MS_CRC_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* Encode message into buffer */
size_t ms_encode(const ms_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (!msg || !buf) return 0;
    size_t total = 7 + msg->payload_len + 2; /* header(7) + payload + CRC */
    if (total > buf_len || msg->payload_len > MS_MAX_PAYLOAD) return 0;

    size_t idx = 0;
    /* Header */
    buf[idx++] = MS_SYNC0;
    buf[idx++] = MS_SYNC1;
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);

    /* Payload */
    memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    /* CRC over all preceding bytes */
    uint16_t crc = ms_crc16(buf, idx);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}

/* Decode buffer into message */
int ms_decode(ms_message_t *msg, const uint8_t *buf, size_t len)
{
    if (!msg || !buf || len < 9) return -1; /* min: 7 header + 2 CRC */

    if (buf[0] != MS_SYNC0 || buf[1] != MS_SYNC1) return -1;

    msg->header.src      = buf[2];
    msg->header.dst      = buf[3];
    msg->header.type     = buf[4];
    msg->header.msg_id   = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);

    msg->payload_len = (uint8_t)(len - 9);
    if (msg->payload_len > MS_MAX_PAYLOAD) return -1;

    memcpy(msg->payload, &buf[7], msg->payload_len);

    uint16_t calc_crc = ms_crc16(buf, len - 2);
    uint16_t recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    if (calc_crc != recv_crc) return -1;

    msg->crc = recv_crc;
    return 0;
}

/* Build acoustic sentinel telemetry payload (14 bytes) */
int ms_build_acoustic_telem(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                             uint8_t battery_v, int8_t temp, uint8_t humidity,
                             uint8_t mosquito_detected, uint8_t species_class,
                             uint8_t confidence, uint16_t wingbeat_freq,
                             uint16_t detections_24h, uint16_t audio_energy,
                             int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = MS_SYNC0;
    msg->header.sync[1] = MS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = MS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = MS_TELEM_ACOUSTIC;
    p[1]  = battery_v;
    p[2]  = (uint8_t)temp;
    p[3]  = humidity;
    p[4]  = mosquito_detected;
    p[5]  = species_class;
    p[6]  = confidence;
    p[7]  = (uint8_t)(wingbeat_freq & 0xFF);
    p[8]  = (uint8_t)(wingbeat_freq >> 8);
    p[9]  = (uint8_t)(detections_24h & 0xFF);
    p[10] = (uint8_t)(detections_24h >> 8);
    p[11] = (uint8_t)(audio_energy & 0xFF);
    p[12] = (uint8_t)(audio_energy >> 8);
    p[13] = (uint8_t)rssi;
    msg->payload_len = 14;

    return 0;
}

/* Build CO2 trap telemetry payload (20 bytes) */
int ms_build_trap_telem(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, int16_t temp_deci, uint16_t humidity_deci,
                        uint16_t pressure_deci, uint16_t rain_tips,
                        uint16_t ir_breaks, uint16_t capture_24h,
                        uint8_t trap_fullness, uint8_t co2_on,
                        uint8_t propane_pct, uint8_t fan_pct,
                        uint8_t dominant_species, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = MS_SYNC0;
    msg->header.sync[1] = MS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = MS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = MS_TELEM_TRAP;
    p[1]  = battery_v;
    p[2]  = (uint8_t)(temp_deci & 0xFF);
    p[3]  = (uint8_t)(temp_deci >> 8);
    p[4]  = (uint8_t)(humidity_deci & 0xFF);
    p[5]  = (uint8_t)(humidity_deci >> 8);
    p[6]  = (uint8_t)(pressure_deci & 0xFF);
    p[7]  = (uint8_t)(pressure_deci >> 8);
    p[8]  = (uint8_t)(rain_tips & 0xFF);
    p[9]  = (uint8_t)(rain_tips >> 8);
    p[10] = (uint8_t)(ir_breaks & 0xFF);
    p[11] = (uint8_t)(ir_breaks >> 8);
    p[12] = (uint8_t)(capture_24h & 0xFF);
    p[13] = (uint8_t)(capture_24h >> 8);
    p[14] = trap_fullness;
    p[15] = co2_on;
    p[16] = propane_pct;
    p[17] = fan_pct;
    p[18] = dominant_species;
    p[19] = (uint8_t)rssi;
    msg->payload_len = 20;

    return 0;
}

/* Build window barrier telemetry payload (8 bytes) */
int ms_build_barrier_telem(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t battery_v, uint8_t screen_status,
                           uint8_t last_trigger, uint8_t cycles_24h,
                           uint16_t motor_current, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = MS_SYNC0;
    msg->header.sync[1] = MS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = MS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0] = MS_TELEM_BARRIER;
    p[1] = battery_v;
    p[2] = screen_status;
    p[3] = last_trigger;
    p[4] = cycles_24h;
    p[5] = (uint8_t)(motor_current & 0xFF);
    p[6] = (uint8_t)(motor_current >> 8);
    p[7] = (uint8_t)rssi;
    msg->payload_len = 8;

    return 0;
}

/* Build weather sentinel telemetry payload (15 bytes) */
int ms_build_weather_telem(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t battery_v, int16_t temp_deci,
                           uint16_t humidity_deci, uint16_t pressure_deci,
                           uint16_t wind_speed_deci, uint16_t wind_dir,
                           uint16_t rain_tips, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = MS_SYNC0;
    msg->header.sync[1] = MS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = MS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = MS_TELEM_WEATHER;
    p[1]  = battery_v;
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
    p[14] = (uint8_t)rssi;
    msg->payload_len = 15;

    return 0;
}

/* Build command message */
int ms_build_command(ms_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len)
{
    if (!msg || (cmd_len > 0 && !cmd_data)) return -1;
    if (cmd_len > MS_MAX_PAYLOAD - 1) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = MS_SYNC0;
    msg->header.sync[1] = MS_SYNC1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = MS_MSG_COMMAND;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = cmd_type;
    if (cmd_len > 0)
        memcpy(&msg->payload[1], cmd_data, cmd_len);
    msg->payload_len = 1 + cmd_len;

    return 0;
}

/* Build alert message */
int ms_build_alert(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len)
{
    if (!msg) return -1;
    if (data_len > MS_MAX_PAYLOAD - 2) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = MS_SYNC0;
    msg->header.sync[1] = MS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = MS_MSG_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = alert_type;
    msg->payload[1] = severity;
    if (data_len > 0)
        memcpy(&msg->payload[2], alert_data, data_len);
    msg->payload_len = 2 + data_len;

    return 0;
}

/* Build risk status broadcast (7 bytes) */
int ms_build_risk_status(ms_message_t *msg, uint8_t src, uint16_t msg_seq,
                          uint8_t risk_level, uint8_t bite_risk_score,
                          uint8_t disease_risk_score, uint8_t activity_index,
                          uint16_t predicted_peak_time_min)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = MS_SYNC0;
    msg->header.sync[1] = MS_SYNC1;
    msg->header.src = src;
    msg->header.dst = MS_BROADCAST;
    msg->header.type = MS_MSG_RISK_STATUS;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = risk_level;          /* 0=low, 1=moderate, 2=high, 3=critical */
    msg->payload[1] = bite_risk_score;     /* 0-100 */
    msg->payload[2] = disease_risk_score;  /* 0-100 */
    msg->payload[3] = activity_index;      /* 0-100 */
    msg->payload[4] = (uint8_t)(predicted_peak_time_min & 0xFF);
    msg->payload[5] = (uint8_t)(predicted_peak_time_min >> 8);
    msg->payload_len = 6;

    return 0;
}

/* Build species alert (immediate mosquito detection notification) */
int ms_build_species_alert(ms_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                           uint8_t species_class, uint8_t confidence,
                           uint16_t wingbeat_freq, uint8_t is_disease_vector)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = MS_SYNC0;
    msg->header.sync[1] = MS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;  /* To hub */
    msg->header.type = MS_MSG_SPECIES_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = species_class;
    msg->payload[1] = confidence;
    msg->payload[2] = (uint8_t)(wingbeat_freq & 0xFF);
    msg->payload[3] = (uint8_t)(wingbeat_freq >> 8);
    msg->payload[4] = is_disease_vector;
    msg->payload_len = 5;

    return 0;
}