/*
 * CompostSync Protocol (CSP)
 * Shared protocol definitions for all nodes
 * csp_protocol.h
 */
#ifndef CSP_PROTOCOL_H
#define CSP_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* Node IDs */
#define NODE_ID_HUB         0x0001
#define NODE_ID_BIN         0x0002
#define NODE_ID_BIN2         0x0003
#define NODE_ID_WEATHER      0x0004
#define NODE_ID_BROADCAST    0xFFFF

/* Message types */
#define CSP_MSG_DATA      0x01
#define CSP_MSG_CMD       0x02
#define CSP_MSG_ACK       0x03
#define CSP_MSG_JOIN      0x04
#define CSP_MSG_SYNC      0x05
#define CSP_MSG_ALERT     0x06

/* Commands (CSP_MSG_CMD payload[0]) */
#define CSP_CMD_OPEN_VENT     0x10
#define CSP_CMD_CLOSE_VENT    0x11
#define CSP_CMD_SET_VENT      0x12  /* payload[1] = 0-100% */
#define CSP_CMD_TARE_WEIGHT   0x20
#define CSP_CMD_REBOOT        0xF0
#define CSP_CMD_OTA_BEGIN     0xF1
#define CSP_CMD_OTA_CHUNK     0xF2
#define CSP_CMD_SET_RATE      0x30  /* payload[1] = sample interval seconds */

/* Alert bitmask flags */
#define ALERT_METHANE_HIGH   0x01
#define ALERT_OVERHEAT       0x02
#define ALERT_LOW_BATTERY    0x04
#define ALERT_SENSOR_FAULT   0x08
#define_ALERT_ANAEROBIC     0x10
#define ALERT_MOISTURE_LOW  0x20
#define ALERT_MOISTURE_HIGH 0x40

/* LoRa parameters */
#define LORA_FREQ            868000000   /* 868 MHz EU ISM */
#define LORA_BW              125000      /* 125 kHz */
#define LORA_SF              11           /* Spreading factor 11 */
#define LORA_CR              5             /* Coding rate 4/5 */
#define LORA_TX_POWER        17            /* 17 dBm (50 mW EU limit) */
#define LORA_PREAMBLE        8
#define LORA_SYNC_WORD      0x34          /* CompostSync sync word */

/* TDMA parameters */
#define TDMA_FRAME_MS        5000
#define TDMA_SLOT_MS         1000
#define TDMA_NUM_SLOTS        5

/* AES */
#define AES_KEY_SIZE         16
#define AES_NONCE_SIZE       12
#define AES_MAC_SIZE         4

/* Packet sizes */
#define CSP_MAX_PAYLOAD      128
#define CSP_HEADER_SIZE      11    /* type(1) + len(1) + seq(1) + src(2) + dst(2) + nonce-pad(4) */
#define CSP_CRC_SIZE         2
#define CSP_MAX_PACKET       (CSP_HEADER_SIZE + CSP_MAX_PAYLOAD + AES_MAC_SIZE + CSP_CRC_SIZE)

/* Compost phases */
#define PHASE_MESOPHILIC     0
#define PHASE_THERMOPHILIC   1
#define PHASE_COOLING        2
#define PHASE_MATURATION     3
#define PHASE_CURED          4
#define PHASE_DORMANT        5
#define PHASE_UNKNOWN        255

#ifdef __cplusplus
extern "C" {
#endif

/* Packet header */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;
    uint8_t  payload_len;
    uint8_t  seq_num;
    uint16_t src_id;
    uint16_t dst_id;
    uint32_t timestamp;
} csp_header_t;

/* Bin node data payload */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    int16_t  temp_c[3];       /* x10 (552 = 55.2°C) */
    uint16_t moisture_pct[3]; /* 0-100 */
    uint16_t co2_ppm;
    uint16_t methane_ppm;
    uint16_t mass_grams;
    uint8_t  vent_position;   /* 0-100 */
    uint8_t  phase;
    uint8_t  alerts;          /* bitmask */
} bin_node_data_t;

/* Weather station data payload */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    int16_t  temp_c;           /* x10 */
    uint16_t humidity_pct;
    uint16_t pressure_hpa;     /* hPa */
    uint16_t wind_speed_ms;    /* x10 */
    uint16_t wind_dir_deg;     /* 0-359 */
    uint16_t rain_mm;          /* x10 (since last report) */
    int8_t   rssi_dbm;
} weather_data_t;

/* Soil probe data payload */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    int16_t  temp_c[4];       /* x10, at 4 depths */
    uint16_t moisture_pct[3]; /* 0-100 */
    int16_t  ph;              /* x100 (650 = 6.50) */
    uint16_t co2_ppm;
    uint8_t  alerts;
} soil_probe_data_t;

/* Command payload */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;
    uint8_t  param[16];
} csp_command_t;

/* Alert payload */
typedef struct __attribute__((packed)) {
    uint8_t  alert_type;
    uint16_t node_id;
    uint8_t  severity;   /* 0=info, 1=warning, 2=critical */
    uint8_t  data[16];
} csp_alert_t;

/* Functions */
uint16_t csp_crc16(const uint8_t *data, size_t len);
int csp_build_packet(uint8_t *buf, uint16_t src, uint16_t dst,
                     uint8_t msg_type, const uint8_t *payload,
                     uint8_t payload_len, uint8_t seq,
                     const uint8_t *aes_key);
int csp_parse_packet(const uint8_t *buf, size_t len,
                     csp_header_t *header, uint8_t *payload,
                     uint8_t *payload_len, const uint8_t *aes_key);
const char *csp_phase_name(uint8_t phase);

#ifdef __cplusplus
}
#endif

#endif /* CSP_PROTOCOL_H */