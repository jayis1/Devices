/*
 * VoiceSync — Protocol Implementation
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#include "protocol.h"

/* CRC-16-CCITT (0xFFFF init, poly 0x1021) */
uint16_t vs_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ VS_CRC_POLY;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* Encode message into buffer */
size_t vs_encode(const vs_message_t *msg, uint8_t *buf, size_t buf_len)
{
    if (!msg || !buf) return 0;
    size_t total = 7 + msg->payload_len + 2; /* header(7) + payload + CRC */
    if (total > buf_len || msg->payload_len > VS_MAX_PAYLOAD) return 0;

    size_t idx = 0;
    /* Header */
    buf[idx++] = VS_SYNC0;
    buf[idx++] = VS_SYNC1;
    buf[idx++] = msg->header.src;
    buf[idx++] = msg->header.dst;
    buf[idx++] = msg->header.type;
    buf[idx++] = (uint8_t)(msg->header.msg_id & 0xFF);
    buf[idx++] = (uint8_t)(msg->header.msg_id >> 8);

    /* Payload */
    memcpy(&buf[idx], msg->payload, msg->payload_len);
    idx += msg->payload_len;

    /* CRC over all preceding bytes */
    uint16_t crc = vs_crc16(buf, idx);
    buf[idx++] = (uint8_t)(crc & 0xFF);
    buf[idx++] = (uint8_t)(crc >> 8);

    return idx;
}

/* Decode buffer into message */
int vs_decode(vs_message_t *msg, const uint8_t *buf, size_t len)
{
    if (!msg || !buf || len < 9) return -1; /* min: 7 header + 2 CRC */

    if (buf[0] != VS_SYNC0 || buf[1] != VS_SYNC1) return -1;

    msg->header.src      = buf[2];
    msg->header.dst      = buf[3];
    msg->header.type     = buf[4];
    msg->header.msg_id   = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);

    msg->payload_len = (uint8_t)(len - 9);
    if (msg->payload_len > VS_MAX_PAYLOAD) return -1;

    memcpy(msg->payload, &buf[7], msg->payload_len);

    uint16_t calc_crc = vs_crc16(buf, len - 2);
    uint16_t recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    if (calc_crc != recv_crc) return -1;

    msg->crc = recv_crc;
    return 0;
}

/* Build vocal band telemetry payload (22 bytes) */
int vs_build_vocal_band_telem(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                              uint8_t battery_v, uint16_t f0_deci,
                              uint16_t jitter_centi, uint16_t shimmer_centi,
                              int8_t hnr_db, uint8_t phonation_pct,
                              uint8_t intensity_db, uint16_t pitch_range_deci,
                              int16_t neck_angle_deci, uint16_t skin_temp_centi,
                              uint8_t heart_rate, uint8_t hrv_rmssd,
                              uint8_t stress_level, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = VS_SYNC0;
    msg->header.sync[1] = VS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = VS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = VS_TELEM_VOCAL_BAND;
    p[1]  = battery_v;
    p[2]  = (uint8_t)(f0_deci & 0xFF);
    p[3]  = (uint8_t)(f0_deci >> 8);
    p[4]  = (uint8_t)(jitter_centi & 0xFF);
    p[5]  = (uint8_t)(jitter_centi >> 8);
    p[6]  = (uint8_t)(shimmer_centi & 0xFF);
    p[7]  = (uint8_t)(shimmer_centi >> 8);
    p[8]  = (uint8_t)hnr_db;
    p[9]  = phonation_pct;
    p[10] = intensity_db;
    p[11] = (uint8_t)(pitch_range_deci & 0xFF);
    p[12] = (uint8_t)(pitch_range_deci >> 8);
    p[13] = (uint8_t)(neck_angle_deci & 0xFF);
    p[14] = (uint8_t)(neck_angle_deci >> 8);
    p[15] = (uint8_t)(skin_temp_centi & 0xFF);
    p[16] = (uint8_t)(skin_temp_centi >> 8);
    p[17] = heart_rate;
    p[18] = hrv_rmssd;
    p[19] = stress_level;
    p[20] = (uint8_t)rssi;
    msg->payload_len = 21;

    return 0;
}

/* Build room sentinel telemetry payload (16 bytes) */
int vs_build_room_telem(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                        uint8_t battery_v, uint8_t voice_quality_class,
                        uint8_t confidence_pct, uint16_t f0_deci,
                        uint8_t phonation_pct, int16_t temp_deci,
                        uint16_t humidity_deci, uint16_t voc_index,
                        uint8_t db_spl, uint8_t talking_detected,
                        int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = VS_SYNC0;
    msg->header.sync[1] = VS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = VS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = VS_TELEM_ROOM;
    p[1]  = battery_v;
    p[2]  = voice_quality_class;
    p[3]  = confidence_pct;
    p[4]  = (uint8_t)(f0_deci & 0xFF);
    p[5]  = (uint8_t)(f0_deci >> 8);
    p[6]  = phonation_pct;
    p[7]  = (uint8_t)(temp_deci & 0xFF);
    p[8]  = (uint8_t)(temp_deci >> 8);
    p[9]  = (uint8_t)(humidity_deci & 0xFF);
    p[10] = (uint8_t)(humidity_deci >> 8);
    p[11] = (uint8_t)(voc_index & 0xFF);
    p[12] = (uint8_t)(voc_index >> 8);
    p[13] = db_spl;
    p[14] = talking_detected;
    p[15] = (uint8_t)rssi;
    msg->payload_len = 16;

    return 0;
}

/* Build hydration tag telemetry payload (10 bytes) */
int vs_build_hydration_telem(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                             uint8_t battery_v, uint16_t water_mass_g,
                             uint16_t sips_24h, uint16_t intake_ml,
                             uint8_t last_sip_min, int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = VS_SYNC0;
    msg->header.sync[1] = VS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = VS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = VS_TELEM_HYDRATION;
    p[1]  = battery_v;
    p[2]  = (uint8_t)(water_mass_g & 0xFF);
    p[3]  = (uint8_t)(water_mass_g >> 8);
    p[4]  = (uint8_t)(sips_24h & 0xFF);
    p[5]  = (uint8_t)(sips_24h >> 8);
    p[6]  = (uint8_t)(intake_ml & 0xFF);
    p[7]  = (uint8_t)(intake_ml >> 8);
    p[8]  = last_sip_min;
    p[9]  = (uint8_t)rssi;
    msg->payload_len = 10;

    return 0;
}

/* Build humidity node telemetry payload (10 bytes) */
int vs_build_humidity_telem(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                            uint8_t battery_v, int16_t temp_deci,
                            uint16_t humidity_deci, uint8_t tank_level_pct,
                            uint8_t humidifier_on, uint8_t fan_on,
                            int8_t rssi)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = VS_SYNC0;
    msg->header.sync[1] = VS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = VS_MSG_TELEMETRY;
    msg->header.msg_id = msg_seq;

    uint8_t *p = msg->payload;
    p[0]  = VS_TELEM_HUMIDITY;
    p[1]  = battery_v;
    p[2]  = (uint8_t)(temp_deci & 0xFF);
    p[3]  = (uint8_t)(temp_deci >> 8);
    p[4]  = (uint8_t)(humidity_deci & 0xFF);
    p[5]  = (uint8_t)(humidity_deci >> 8);
    p[6]  = tank_level_pct;
    p[7]  = humidifier_on;
    p[8]  = fan_on;
    p[9]  = (uint8_t)rssi;
    msg->payload_len = 10;

    return 0;
}

/* Build command message */
int vs_build_command(vs_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len)
{
    if (!msg || (cmd_len > 0 && !cmd_data)) return -1;
    if (cmd_len > VS_MAX_PAYLOAD - 1) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = VS_SYNC0;
    msg->header.sync[1] = VS_SYNC1;
    msg->header.src = src;
    msg->header.dst = dst;
    msg->header.type = VS_MSG_COMMAND;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = cmd_type;
    if (cmd_len > 0)
        memcpy(&msg->payload[1], cmd_data, cmd_len);
    msg->payload_len = 1 + cmd_len;

    return 0;
}

/* Build alert message */
int vs_build_alert(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len)
{
    if (!msg) return -1;
    if (data_len > VS_MAX_PAYLOAD - 2) return -1;

    memset(msg, 0, sizeof(*msg));
    msg->header.sync[0] = VS_SYNC0;
    msg->header.sync[1] = VS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;
    msg->header.type = VS_MSG_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = alert_type;
    msg->payload[1] = severity;
    if (data_len > 0)
        memcpy(&msg->payload[2], alert_data, data_len);
    msg->payload_len = 2 + data_len;

    return 0;
}

/* Build voice status broadcast (9 bytes) */
int vs_build_voice_status(vs_message_t *msg, uint8_t src, uint16_t msg_seq,
                          uint8_t risk_level, uint8_t vocal_health_score,
                          uint8_t disorder_risk, uint8_t phonation_pct_today,
                          uint8_t hydration_pct, uint8_t rest_recommended,
                          uint16_t rest_minutes_remaining)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = VS_SYNC0;
    msg->header.sync[1] = VS_SYNC1;
    msg->header.src = src;
    msg->header.dst = VS_BROADCAST;
    msg->header.type = VS_MSG_VOICE_STATUS;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = risk_level;            /* 0=low, 1=moderate, 2=high, 3=critical */
    msg->payload[1] = vocal_health_score;    /* 0-100 */
    msg->payload[2] = disorder_risk;          /* 0-100 */
    msg->payload[3] = phonation_pct_today;   /* 0-100 */
    msg->payload[4] = hydration_pct;         /* 0-100 */
    msg->payload[5] = rest_recommended;      /* 0/1 */
    msg->payload[6] = (uint8_t)(rest_minutes_remaining & 0xFF);
    msg->payload[7] = (uint8_t)(rest_minutes_remaining >> 8);
    msg->payload_len = 8;

    return 0;
}

/* Build voice alert (immediate voice quality notification) */
int vs_build_voice_alert(vs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                         uint8_t voice_quality_class, uint8_t confidence,
                         uint16_t f0_deci, uint8_t is_critical)
{
    if (!msg) return -1;
    memset(msg, 0, sizeof(*msg));

    msg->header.sync[0] = VS_SYNC0;
    msg->header.sync[1] = VS_SYNC1;
    msg->header.src = node_id;
    msg->header.dst = 0x00;  /* To hub */
    msg->header.type = VS_MSG_VOICE_ALERT;
    msg->header.msg_id = msg_seq;

    msg->payload[0] = voice_quality_class;
    msg->payload[1] = confidence;
    msg->payload[2] = (uint8_t)(f0_deci & 0xFF);
    msg->payload[3] = (uint8_t)(f0_deci >> 8);
    msg->payload[4] = is_critical;
    msg->payload_len = 5;

    return 0;
}