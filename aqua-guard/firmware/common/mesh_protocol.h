/*
 * mesh_protocol.h — Shared Sub-GHz mesh protocol definitions
 *
 * Used by all Aqua Guard nodes (hub, sensor, feeder)
 * TDMA: 10 slots × 100ms = 1 second frame
 */

#pragma once

#include <stdint.h>

/* Node IDs */
#define NODE_ID_HUB        0x00
#define NODE_ID_BROADCAST  0xFF
#define NODE_ID_SENSOR_MIN 0x01
#define NODE_ID_SENSOR_MAX 0x07
#define NODE_ID_FEEDER     0x08

/* TDMA timing */
#define TDMA_FRAME_MS       1000
#define TDMA_SLOT_MS        100
#define TDMA_NUM_SLOTS      10
#define TDMA_GUARD_US       500   /* guard time between slots */

/* Packet types */
#define PKT_SENSOR_DATA     0x01
#define PKT_FEEDER_STATUS   0x02
#define PKT_COMMAND         0x03
#define PKT_ACK             0x04
#define PKT_OTA_BLOCK       0x05
#define PKT_CALIBRATION     0x06
#define PKT_ALARM           0x07
#define PKT_HEARTBEAT       0x08

/* Sync word */
#define MESH_SYNC_WORD      0xA5A5

/* Max payload size (fits in LoRa SF7 125kHz) */
#define MESH_MAX_PAYLOAD    50

/* Packet structure (over the air) */
typedef struct __attribute__((packed)) {
    uint8_t  preamble[4];   /* 0xAA 0xAA 0xAA 0xAA */
    uint16_t sync;          /* MESH_SYNC_WORD */
    uint8_t  len;           /* payload length */
    uint8_t  src_id;        /* source node ID */
    uint8_t  dst_id;        /* destination node ID (0xFF=broadcast) */
    uint8_t  pkt_type;      /* PKT_* type */
    uint8_t  payload[MESH_MAX_PAYLOAD];
    uint16_t crc16;         /* CRC-16/CCITT over len+src+dst+type+payload */
} mesh_packet_t;

/* Sensor data payload (8 × 4-byte floats = 32 bytes) */
typedef struct __attribute__((packed)) {
    float ph;           /* 0.0 - 14.0 */
    float temperature;  /* -10.0 - 50.0 °C */
    float ammonia;      /* 0.0 - 10.0 ppm NH3 */
    float nitrite;      /* 0.0 - 10.0 ppm NO2 */
    float nitrate;      /* 0.0 - 100.0 ppm NO3 */
    float dissolved_o2; /* 0.0 - 20.0 mg/L */
    float tds;          /* 0.0 - 50000 µS/cm */
    float turbidity;    /* 0.0 - 3000 NTU */
} sensor_data_payload_t;

/* Feeder status payload (24 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  pump_states;     /* bitfield: pump 1-6 on/off */
    uint16_t flow_rates[6];  /* mL/min per pump */
    uint8_t  hopper_level;   /* 0-100% */
    uint8_t  led_r, led_g, led_b, led_w;  /* 0-255 per channel */
    uint8_t  camera_ready;   /* 0=idle, 1=streaming */
    uint8_t  temp_c;         /* pump compartment temp */
    uint8_t  alarms;         /* alarm bitfield */
} feeder_status_payload_t;

/* Command payload (variable) */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;    /* CMD_DOSE, CMD_FEED, CMD_LIGHT, CMD_ALARM */
    uint8_t  param_len;   /* length of following params */
    uint8_t  params[16];  /* command-specific parameters */
} command_payload_t;

/* Command types */
#define CMD_DOSE       0x01  /* params: [pump_id(1), volume_mL(2)] */
#define CMD_FEED       0x02  /* params: [portions(1)] */
#define CMD_LIGHT      0x03  /* params: [r(1), g(1), b(1), w(1)] */
#define CMD_ALARM_OFF  0x04  /* params: [alarm_mask(1)] */
#define CMD_CAMERA     0x05  /* params: [mode(1): 0=off, 1=single, 2=stream] */
#define CMD_CALIBRATE  0x06  /* params: [sensor_id(1), value(4)] */

/* CRC-16/CCITT calculation */
uint16_t mesh_crc16(const uint8_t *data, uint16_t len);

/* Build a mesh packet */
uint16_t mesh_build_packet(uint8_t src, uint8_t dst, uint8_t type,
                            const uint8_t *payload, uint8_t payload_len,
                            mesh_packet_t *out);

/* Parse and validate a received packet */
int8_t mesh_parse_packet(const uint8_t *raw, uint16_t raw_len, mesh_packet_t *out);