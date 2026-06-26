/*
 * BrewSync Mesh Protocol (BSMP) - Common Protocol Implementation
 * Shared across all nodes and hub
 *
 * Copyright (c) 2025 BrewSync. MIT License.
 */

#ifndef BSMP_H
#define BSMP_H

#include <stdint.h>
#include <string.h>

/* ---- Constants ---- */
#define BSMP_PREAMBLE       0xAA55AA55UL
#define BSMP_ADDR_HUB       0x0000
#define BSMP_ADDR_BROADCAST 0xFFFF
#define BSMP_MAX_PAYLOAD    200
#define BSMP_FRAME_OVERHEAD 11   /* preamble(4)+addr(2)+seq(1)+type(1)+len(1)+crc(2) */
#define BSMP_RETRIES        3
#define BSMP_RETRY_DELAY_MS 1000

/* Frame types */
#define BSMP_TYPE_BEACON      0x01
#define BSMP_TYPE_PAIR_REQ    0x02
#define BSMP_TYPE_PAIR_RESP   0x03
#define BSMP_TYPE_TELEMETRY   0x10
#define BSMP_TYPE_COMMAND     0x11
#define BSMP_TYPE_ACK         0x20
#define BSMP_TYPE_ALERT       0x30
#define BSMP_TYPE_OTA_INIT    0x40
#define BSMP_TYPE_OTA_DATA    0x41
#define BSMP_TYPE_OTA_DONE    0x42
#define BSMP_TYPE_HEARTBEAT   0xF0

/* Command codes */
#define BSMP_CMD_SET_REPORT_INTERVAL  0x01
#define BSMP_CMD_SET_SF               0x02
#define BSMP_CMD_START_BATCH          0x03
#define BSMP_CMD_END_BATCH            0x04
#define BSMP_CMD_CALIBRATE            0x05
#define BSMP_CMD_RESET                0x06
#define BSMP_CMD_SET_TEMP_TARGET      0x07
#define BSMP_CMD_ENABLE_RELAY         0x08

/* Alert types */
#define BSMP_ALERT_TEMP_EXCURSION     0x01
#define BSMP_ALERT_STUCK_FERMENT      0x02
#define BSMP_ALERT_INFECTION          0x03
#define BSMP_ALERT_BATTERY_LOW        0x04
#define BSMP_ALERT_SENSOR_FAULT       0x05
#define BSMP_ALERT_TARGET_FG          0x06
#define BSMP_ALERT_FERMENT_DONE       0x07

/* Alert severity */
#define BSMP_SEVERITY_INFO     0
#define BSMP_SEVERITY_WARNING  1
#define BSMP_SEVERITY_CRITICAL 2

/* Node types */
#define BSMP_NODE_FERMENTER  1
#define BSMP_NODE_CELLAR     2
#define BSMP_NODE_SCANNER    3

/* Telemetry flags */
#define BSMP_FLAG_TEMP_ALARM  (1 << 0)
#define BSMP_FLAG_SG_ALARM   (1 << 1)
#define BSMP_FLAG_CO2_ALARM  (1 << 2)
#define BSMP_FLAG_PH_ALARM   (1 << 3)

/* Sensor status bits */
#define BSMP_SENS_SG_OK      (1 << 0)
#define BSMP_SENS_TEMP_OK    (1 << 1)
#define BSMP_SENS_CO2_OK     (1 << 2)
#define BSMP_SENS_PRESS_OK   (1 << 3)
#define BSMP_SENS_PH_OK      (1 << 4)

/* ---- Frame structure ---- */
typedef struct __attribute__((packed)) {
    uint32_t preamble;
    uint16_t addr;
    uint8_t  seq;
    uint8_t  type;
    uint8_t  len;
    uint8_t  payload[BSMP_MAX_PAYLOAD];
    uint16_t crc;
} bsmp_frame_t;

/* ---- Fermenter telemetry payload ---- */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    float    sg;          /* Specific gravity e.g. 1.065 */
    float    temp_c;      /* Temperature in °C */
    float    co2_ppm;     /* CO2 concentration ppm */
    float    pressure_bar;/* Gauge pressure in bar */
    float    ph;          /* pH value */
    uint16_t battery_mv;  /* Battery voltage in mV */
    uint8_t  flags;       /* Alarm flags */
    uint8_t  sensor_status;/* Sensor health bitmask */
} bsmp_fermenter_telem_t;

/* ---- Cellar telemetry payload ---- */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    float    temp_c;
    float    humidity_rh;
    float    pressure_hpa;
    float    vibration_rms_mg;
    uint16_t light_lux;
    uint16_t battery_mv;
    uint8_t  flags;
} bsmp_cellar_telem_t;

/* ---- Command payload ---- */
typedef struct __attribute__((packed)) {
    uint8_t cmd;
    uint8_t param_len;
    uint8_t params[32];   /* Variable length, param_len bytes valid */
} bsmp_command_t;

/* ---- Alert payload ---- */
typedef struct __attribute__((packed)) {
    uint8_t alert_type;
    uint8_t severity;
    uint8_t msg_len;
    char    message[64];
} bsmp_alert_t;

/* ---- CRC-16/CCITT ---- */
static inline uint16_t bsmp_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ---- Frame encode ---- */
static inline int bsmp_encode(uint16_t addr, uint8_t seq, uint8_t type,
                               const uint8_t *payload, uint8_t payload_len,
                               uint8_t *out_buf, uint16_t out_buf_size) {
    if (payload_len > BSMP_MAX_PAYLOAD)
        return -1;
    if (out_buf_size < (uint16_t)(BSMP_FRAME_OVERHEAD + payload_len))
        return -2;

    uint16_t idx = 0;
    /* Preamble */
    out_buf[idx++] = (BSMP_PREAMBLE >> 24) & 0xFF;
    out_buf[idx++] = (BSMP_PREAMBLE >> 16) & 0xFF;
    out_buf[idx++] = (BSMP_PREAMBLE >> 8) & 0xFF;
    out_buf[idx++] = (BSMP_PREAMBLE) & 0xFF;
    /* Address */
    out_buf[idx++] = (addr >> 8) & 0xFF;
    out_buf[idx++] = addr & 0xFF;
    /* Sequence */
    out_buf[idx++] = seq;
    /* Type */
    out_buf[idx++] = type;
    /* Length */
    out_buf[idx++] = payload_len;
    /* Payload */
    if (payload_len > 0 && payload)
        memcpy(&out_buf[idx], payload, payload_len);
    idx += payload_len;
    /* CRC over addr through payload */
    uint16_t crc = bsmp_crc16(&out_buf[4], idx - 4);
    out_buf[idx++] = (crc >> 8) & 0xFF;
    out_buf[idx++] = crc & 0xFF;

    return idx;
}

/* ---- Frame decode ---- */
static inline int bsmp_decode(const uint8_t *buf, uint16_t buf_len,
                               bsmp_frame_t *frame) {
    if (buf_len < BSMP_FRAME_OVERHEAD)
        return -1;

    /* Check preamble */
    uint32_t preamble = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                        ((uint32_t)buf[2] << 8) | buf[3];
    if (preamble != BSMP_PREAMBLE)
        return -2;

    frame->addr = ((uint16_t)buf[4] << 8) | buf[5];
    frame->seq  = buf[6];
    frame->type = buf[7];
    frame->len  = buf[8];

    if (frame->len > BSMP_MAX_PAYLOAD)
        return -3;
    if (buf_len < (uint16_t)(BSMP_FRAME_OVERHEAD + frame->len))
        return -4;

    memcpy(frame->payload, &buf[9], frame->len);

    /* Verify CRC */
    uint16_t received_crc = ((uint16_t)buf[9 + frame->len] << 8) |
                             buf[9 + frame->len + 1];
    uint16_t computed_crc = bsmp_crc16(&buf[4], 5 + frame->len);
    if (received_crc != computed_crc)
        return -5;

    frame->crc = received_crc;
    return 0;
}

/* ---- AES-128-CCM stub (platform must implement) ---- */
/* Actual AES-128-CCM implementation is platform-specific.
 * The following functions must be implemented by the hardware
 * abstraction layer (HAL) for each platform. */
int bsmp_aes128_ccm_encrypt(const uint8_t key[16], const uint8_t nonce[12],
                            const uint8_t *adata, uint8_t adata_len,
                            const uint8_t *plaintext, uint8_t pt_len,
                            uint8_t *ciphertext, uint8_t *tag, uint8_t tag_len);

int bsmp_aes128_ccm_decrypt(const uint8_t key[16], const uint8_t nonce[12],
                            const uint8_t *adata, uint8_t adata_len,
                            const uint8_t *ciphertext, uint8_t ct_len,
                            const uint8_t *tag, uint8_t tag_len,
                            uint8_t *plaintext);

#endif /* BSMP_H */