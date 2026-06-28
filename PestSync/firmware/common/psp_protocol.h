/*
 * PestSync Protocol (PSP)
 * Shared protocol definitions for all nodes
 * psp_protocol.h
 */
#ifndef PSP_PROTOCOL_H
#define PSP_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* Node IDs */
#define NODE_ID_HUB           0x0001
#define NODE_ID_SENTINEL_BASE  0x0010  /* Sentinels: 0x0010-0x001F */
#define NODE_ID_TRAP_BASE      0x0020  /* Traps: 0x0020-0x002F */
#define NODE_ID_DETERRENT_BASE 0x0030  /* Deterrents: 0x0030-0x003F */
#define NODE_ID_BROADCAST      0xFFFF

/* Message types */
#define PSP_MSG_DATA      0x01
#define PSP_MSG_CMD       0x02
#define PSP_MSG_ACK       0x03
#define PSP_MSG_JOIN      0x04
#define PSP_MSG_SYNC      0x05
#define PSP_MSG_ALERT     0x06

/* Commands (PSP_MSG_CMD payload[0]) */
#define PSP_CMD_REBOOT         0xF0
#define PSP_CMD_OTA_BEGIN      0xF1
#define PSP_CMD_OTA_CHUNK      0xF2
#define PSP_CMD_SET_RATE       0x30  /* payload[1-4] = sample interval seconds */
#define PSP_CMD_TRIGGER_CAMERA 0x40 /* sentinel: capture now */
#define PSP_CMD_SET_DETER      0x50 /* deterrent: payload[1]=mode, [2]=freq_band, [3-4]=duration_s */
#define PSP_CMD_DETER_OFF      0x51 /* deterrent: turn off all */
#define PSP_CMD_DETER_STROBE   0x52 /* deterrent: trigger strobe burst */
#define PSP_CMD_DETER_DIFFUSE  0x53 /* deterrent: trigger one diffuser dose */
#define PSP_CMD_RESET_TRAP     0x60 /* trap: mark reset/rearmed */

/* Alert bitmask flags */
#define ALERT_PEST_DETECTED    0x01
#define ALERT_TRAP_TRIGGERED   0x02
#define ALERT_TRAP_TAMPERED    0x04
#define ALERT_LOW_BATTERY      0x08
#define ALERT_SENSOR_FAULT     0x10
#define ALERT_BAIT_LOW         0x20
#define ALERT_OIL_LOW          0x40
#define ALERT_TERMITE_SWARM    0x80  /* critical — call professional */

/* Sub-GHz parameters */
#define LORA_FREQ            868000000   /* 868 MHz EU ISM */
#define LORA_BW              125000      /* 125 kHz */
#define LORA_SF              11           /* Spreading factor 11 */
#define LORA_CR              5             /* Coding rate 4/5 */
#define LORA_TX_POWER        17            /* 17 dBm (50 mW EU limit) */
#define LORA_PREAMBLE        8
#define LORA_SYNC_WORD       0x3C          /* PestSync sync word */

/* TDMA parameters */
#define TDMA_FRAME_MS        8000
#define TDMA_SLOT_MS         1000
#define TDMA_NUM_SLOTS       8

/* AES */
#define AES_KEY_SIZE         16
#define AES_NONCE_SIZE       12
#define AES_MAC_SIZE         4

/* Packet sizes */
#define PSP_MAX_PAYLOAD      128
#define PSP_HEADER_SIZE      11    /* type(1) + len(1) + seq(1) + src(2) + dst(2) + timestamp(4) */
#define PSP_CRC_SIZE         2
#define PSP_MAX_PACKET       (PSP_HEADER_SIZE + PSP_MAX_PAYLOAD + AES_MAC_SIZE + PSP_CRC_SIZE)

/* Pest classes (YOLOv8-nano output) */
#define PEST_NONE            255
#define PEST_HOUSE_MOUSE      0
#define PEST_NORWAY_RAT       1
#define PEST_GERMAN_ROACH     2
#define PEST_AMERICAN_ROACH   3
#define PEST_ARGENTINE_ANT    4
#define PEST_CARPENTER_ANT    5
#define PEST_MOSQUITO         6
#define PEST_HOUSE_FLY        7
#define PEST_FRUIT_FLY        8
#define PEST_BEDBUG           9
#define PEST_TERMITE_WORKER  10
#define PEST_TERMITE_SWARMER 11
#define PEST_SPIDER          12
#define PEST_SILVERFISH      13
#define PEST_CARPET_BEETLE   14
#define PEST_CLASS_COUNT     15

/* Trap catch classification */
#define CATCH_UNKNOWN      255
#define CATCH_MOUSE          0
#define CATCH_RAT           1
#define CATCH_INSECT        2
#define CATCH_FALSE_TRIGGER 3

/* Trap status */
#define TRAP_ARMED         0
#define TRAP_TRIGGERED     1
#define TRAP_NEEDS_RESET   2
#define TRAP_TAMPERED      3

/* Deterrent modes */
#define DETER_OFF          0
#define DETER_SCHEDULE     1
#define DETER_ADAPTIVE     2
#define DETER_ALWAYS_ON    3

/* Deterrent frequency bands */
#define DETER_BAND_RODENT  0  /* 20-30 kHz */
#define DETER_BAND_INSECT  1  /* 40-60 kHz */
#define DETER_BAND_BOTH    2  /* 20-65 kHz sweep */

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
} psp_header_t;

/* Pest Sentinel detection payload */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    uint8_t  pest_class;       /* 0-14 (pest species), 255 = none */
    uint8_t  confidence;       /* 0-100% */
    uint16_t count_since_last; /* detections since last report */
    int16_t  thermal_max_c;   /* x10, max thermal pixel */
    uint8_t  ir_illumination; /* was IR LED on? */
    uint8_t  alerts;          /* bitmask */
} sentinel_data_t;

/* Smart Trap event payload */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    uint8_t  trap_status;     /* 0=armed, 1=triggered, 2=needs_reset, 3=tampered */
    uint16_t catch_weight_g;  /* HX711 reading */
    uint8_t  bait_level;      /* 0=empty, 50=low, 100=ok */
    uint8_t  catch_class;    /* 0=mouse, 1=rat, 2=insect, 3=false_trigger, 255=unknown */
    uint8_t  alerts;
} trap_data_t;

/* Deterrent status payload */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    uint8_t  ultrasonic_active;
    uint8_t  strobe_active;
    uint8_t  diffuser_active;
    uint8_t  oil_level;       /* 0-100% */
    uint32_t total_ultrasonic_s;
    uint16_t diffuser_doses;
    uint8_t  alerts;
} deterrent_data_t;

/* Command payload */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;
    uint8_t  param[16];
} psp_command_t;

/* Alert payload */
typedef struct __attribute__((packed)) {
    uint8_t  alert_type;
    uint16_t node_id;
    uint8_t  severity;   /* 0=info, 1=warning, 2=critical */
    uint8_t  data[16];
} psp_alert_t;

/* Functions */
uint16_t psp_crc16(const uint8_t *data, size_t len);
int psp_build_packet(uint8_t *buf, uint16_t src, uint16_t dst,
                     uint8_t msg_type, const uint8_t *payload,
                     uint8_t payload_len, uint8_t seq,
                     const uint8_t *aes_key);
int psp_parse_packet(const uint8_t *buf, size_t len,
                     psp_header_t *header, uint8_t *payload,
                     uint8_t *payload_len, const uint8_t *aes_key);
const char *pest_class_name(uint8_t pest_class);
const char *trap_status_name(uint8_t status);

#ifdef __cplusplus
}
#endif

#endif /* PSP_PROTOCOL_H */