/*
 * HiveSync Protocol — Shared header for all nodes
 * Defines frame format, message types, packing/unpacking
 */

#ifndef HIVESYNC_PROTO_H
#define HIVESYNC_PROTO_H

#include <stdint.h>
#include <string.h>

/* ---- Node Types ---- */
#define NODE_SENSOR            0x01
#define NODE_ENTRANCE_MONITOR  0x02
#define NODE_SMART_FEEDER      0x03
#define NODE_GATEWAY           0x04

/* ---- Message Types ---- */
#define MSG_BEACON         0x01
#define MSG_DATA           0x02
#define MSG_AUDIO_FEATURES 0x03
#define MSG_WEIGHT_DELTA   0x04
#define MSG_COMMAND        0x05
#define MSG_IMAGE_THUMB    0x06
#define MSG_OTA_BLOCK      0x07
#define MSG_ACK            0x08
#define MSG_ALARM          0x09
#define MSG_FEEDER_STATUS  0x0A

/* ---- Command IDs ---- */
#define CMD_DISPENSE_SYRUP  0x01
#define CMD_ADVANCE_PATTY   0x02
#define CMD_VALVE_OPEN      0x03
#define CMD_VALVE_CLOSE     0x04
#define CMD_SET_INTERVAL    0x05
#define CMD_OTA_START       0x06
#define CMD_REBOOT          0x07

/* ---- Command Results ---- */
#define CMD_OK              0x00
#define CMD_PARTIAL         0x01
#define CMD_CLOG_DETECTED   0x02
#define CMD_ERROR           0xFF

/* ---- Frame Header (8 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  preamble[2];    /* 0xAA, 0x55 */
    uint8_t  msg_type;
    uint16_t src_id;
    uint16_t dst_id;         /* 0x0000 = gateway */
    uint16_t length;         /* Payload length */
    uint8_t  version;        /* FW version */
} hivesync_header_t;

#define MAX_DETECTIONS 20

/* ---- Detection structures ---- */
typedef enum {
    BEE_DIR_IN = 0,
    BEE_DIR_OUT = 1,
    BEE_DIR_UNKNOWN = 2
} bee_direction_t;

typedef struct {
    float x, y, w, h;       /* Bounding box */
    float confidence;
    bee_direction_t direction;
} bee_detection_t;

typedef struct {
    float mites_per_bee;
    int   mite_class;         /* 0=none, 1=low, 2=moderate, 3=high */
} varroa_result_t;

/* ---- Sensor Data Payload ---- */
typedef struct {
    float temp_brood;
    float temp_top;
    float temp_entrance;
    float humidity;
    float weight_kg;
    float weight_delta_g;
    float accel_rms_mg;
    float battery_mv;
    float audio_centroid;
    float audio_peak_freq;
    float audio_peak_amp_db;
    float audio_bandwidth;
} sensor_data_payload_t;

/* ---- Parse Result ---- */
typedef struct {
    uint8_t  msg_type;
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t  node_type;
    sensor_data_payload_t data;
    struct {
        uint8_t cmd_id;
        uint16_t param_u16;
        float    param_f32;
    } cmd;
} hivesync_msg_t;

#define PARSE_OK    0
#define PARSE_ERROR -1

/* ---- Packing Functions ---- */
static inline uint16_t hivesync_pack_header(uint8_t *buf, uint8_t msg_type,
    uint16_t src_id, uint16_t dst_id, uint16_t payload_len, uint8_t version) {
    buf[0] = 0xAA; buf[1] = 0x55;
    buf[2] = msg_type;
    buf[3] = (src_id >> 8) & 0xFF; buf[4] = src_id & 0xFF;
    buf[5] = (dst_id >> 8) & 0xFF; buf[6] = dst_id & 0xFF;
    buf[7] = version;
    buf[8] = (payload_len >> 8) & 0xFF; buf[9] = payload_len & 0xFF;
    return 10;
}

static inline int hivesync_parse(const uint8_t *buf, uint16_t len, hivesync_msg_t *msg) {
    if (len < 10) return PARSE_ERROR;
    if (buf[0] != 0xAA || buf[1] != 0x55) return PARSE_ERROR;

    msg->msg_type = buf[2];
    msg->src_id   = (buf[3] << 8) | buf[4];
    msg->dst_id   = (buf[5] << 8) | buf[6];
    uint8_t version = buf[7];
    uint16_t payload_len = (buf[8] << 8) | buf[9];

    if (msg->msg_type == MSG_DATA && payload_len >= sizeof(sensor_data_payload_t)) {
        const float *p = (const float *)(buf + 10);
        msg->data.temp_brood      = p[0];
        msg->data.temp_top         = p[1];
        msg->data.temp_entrance    = p[2];
        msg->data.humidity         = p[3];
        msg->data.weight_kg        = p[4];
        msg->data.weight_delta_g   = p[5];
        msg->data.accel_rms_mg     = p[6];
        msg->data.battery_mv       = p[7];
        msg->data.audio_centroid   = p[8];
        msg->data.audio_peak_freq  = p[9];
        msg->data.audio_peak_amp_db = p[10];
        msg->data.audio_bandwidth  = p[11];
    } else if (msg->msg_type == MSG_COMMAND && payload_len >= 7) {
        msg->cmd.cmd_id    = buf[10];
        msg->cmd.param_u16 = (buf[11] << 8) | buf[12];
        memcpy(&msg->cmd.param_f32, buf + 13, 4);
    }

    return PARSE_OK;
}

static inline uint16_t hivesync_pack_data(uint8_t *buf,
    hivesync_header_t *hdr,
    const float *temps, int n_temps,
    float humidity, float weight_kg, float weight_delta_g,
    float accel_rms, float battery_mv,
    float centroid, float peak_freq, float peak_amp, float bandwidth) {
    uint16_t hdr_len = hivesync_pack_header(buf, MSG_DATA, hdr->src_id, hdr->dst_id, 48, hdr->version);
    float *p = (float *)(buf + hdr_len);
    p[0]  = temps[0];  /* brood */
    p[1]  = temps[1];  /* top */
    p[2]  = temps[2];  /* entrance */
    p[3]  = humidity;
    p[4]  = weight_kg;
    p[5]  = weight_delta_g;
    p[6]  = accel_rms;
    p[7]  = battery_mv;
    p[8]  = centroid;
    p[9]  = peak_freq;
    p[10] = peak_amp;
    p[11] = bandwidth;
    /* CRC16 */
    uint16_t total = hdr_len + 48;
    uint16_t crc = 0;
    for (int i = 0; i < total; i++) crc ^= buf[i];
    buf[total] = crc & 0xFF;
    buf[total+1] = (crc >> 8) & 0xFF;
    return total + 2;
}

static inline uint16_t hivesync_pack_beacon(uint8_t *buf, uint16_t gw_id, uint8_t max_nodes) {
    hivesync_header_t hdr = {.src_id = gw_id, .dst_id = 0xFFFF, .version = 1};
    uint16_t hdr_len = hivesync_pack_header(buf, MSG_BEACON, gw_id, 0xFFFF, 4, 1);
    buf[10] = max_nodes;
    buf[11] = 0; /* reserved */
    buf[12] = 0;
    buf[13] = 0;
    return 14;
}

static inline uint16_t hivesync_pack_entrance(uint8_t *buf, uint16_t node_id,
    const void *data) {
    typedef struct {
        int bees_in, bees_out;
        float mites_per_bee;
        int mite_class;
        float temp_c, humidity;
    } entrance_payload_t;
    const entrance_payload_t *d = (const entrance_payload_t *)data;
    hivesync_header_t hdr = {.src_id = node_id, .dst_id = 0x0000, .version = 1};
    uint16_t hdr_len = hivesync_pack_header(buf, MSG_DATA, node_id, 0x0000, 20, 1);
    float *p = (float *)(buf + hdr_len);
    p[0] = (float)d->bees_in;
    p[1] = (float)d->bees_out;
    p[2] = d->mites_per_bee;
    p[3] = (float)d->mite_class;
    p[4] = d->temp_c;
    p[5] = 0; /* reserved */
    return hdr_len + 24 + 2; /* +2 for CRC */
}

static inline uint16_t hivesync_pack_feeder(uint8_t *buf, uint16_t node_id,
    float weight_kg, float temp_c, float humidity_pct,
    uint8_t state, uint8_t valve_open, uint8_t clog,
    float battery_mv, uint16_t dispense_count) {
    hivesync_header_t hdr = {.src_id = node_id, .dst_id = 0x0000, .version = 1};
    uint16_t hdr_len = hivesync_pack_header(buf, MSG_FEEDER_STATUS, node_id, 0x0000, 24, 1);
    float *p = (float *)(buf + hdr_len);
    p[0] = weight_kg;
    p[1] = temp_c;
    p[2] = humidity_pct;
    p[3] = battery_mv;
    buf[hdr_len + 16] = state;
    buf[hdr_len + 17] = valve_open;
    buf[hdr_len + 18] = clog;
    buf[hdr_len + 19] = (dispense_count >> 8) & 0xFF;
    buf[hdr_len + 20] = dispense_count & 0xFF;
    return hdr_len + 22;
}

static inline void hivesync_send_beacon(void *radio, uint16_t node_id, uint8_t node_type) {
    uint8_t buf[14];
    hivesync_header_t hdr = {.src_id = node_id, .dst_id = 0x0000, .version = 1};
    hivesync_pack_header(buf, MSG_BEACON, node_id, 0x0000, 4, 1);
    buf[10] = node_type;
    buf[11] = 0;
    buf[12] = 0;
    buf[13] = 0;
    /* Radio TX handled by caller */
}

static inline void hivesync_handle_command(const uint8_t *buf, uint16_t len) {
    hivesync_msg_t msg;
    if (hivesync_parse(buf, len, &msg) == PARSE_OK && msg.msg_type == MSG_COMMAND) {
        /* Command handling is node-specific — override in each node */
    }
}

#endif /* HIVESYNC_PROTO_H */