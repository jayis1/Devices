/*
 * mesh_protocol.h — Shared BLE mesh protocol definitions for SleepSync
 *
 * Used by all SleepSync nodes (nightstand hub, sleep strip, climate, shade)
 * BLE 5.0 mesh with custom vendor models for sleep-specific data
 */

#pragma once

#include <stdint.h>

/* ---- Node IDs ---- */
#define NODE_ID_HUB        0x00
#define NODE_ID_BROADCAST  0xFF
#define NODE_ID_STRIP      0x01
#define NODE_ID_CLIMATE    0x02
#define NODE_ID_SHADE_MIN  0x03
#define NODE_ID_SHADE_MAX  0x06  /* up to 4 shade controllers */

/* ---- Mesh Message Types (vendor model opcodes) ---- */
#define MSG_SLEEP_DATA     0x01  /* Strip → Hub: HR, RR, movement, snoring */
#define MSG_ENV_DATA       0x02  /* Climate → Hub: temp, RH, CO2, HVAC state */
#define MSG_SHADE_STATUS   0x03  /* Shade → Hub: position, light, LED state */
#define MSG_HUB_COMMAND    0x04  /* Hub → Climate/Shade: setpoints, open/close */
#define MSG_HUB_SYNC       0x05  /* Hub → All: clock sync, mesh params, OTA */
#define MSG_ALARM_TRIGGER  0x06  /* Hub → All: alarm event */
#define MSG_ACK            0x07  /* Any → Hub: command acknowledgment */
#define MSG_OTA_BLOCK      0x08  /* Hub → Any: firmware update chunk */

/* ---- BLE Mesh Parameters ---- */
#define MESH_NETKEY_INDEX  0
#define MESH_APPKEY_INDEX  0
#define MESH_IV_INDEX      0

/* ---- Sleep Data Payload (11 bytes, Strip → Hub every 5s) ---- */
typedef struct __attribute__((packed)) {
    uint16_t heart_rate;      /* BPM ×10, e.g. 450 = 45.0 BPM */
    uint8_t  hrv;             /* Heart rate variability (0-255) */
    uint16_t resp_rate;       /* Breaths/min ×10, e.g. 160 = 16.0/min */
    uint8_t  rrv;             /* Respiration rate variability (0-255) */
    uint8_t  movement;        /* Movement intensity (0-255) */
    uint8_t  snoring;         /* Snoring intensity (0-255, >100=snoring) */
    uint8_t  sleep_stage;     /* 0=awake, 1=light, 2=deep, 3=REM */
    uint8_t  stage_conf;      /* Stage confidence (0-255, ×100=%) */
    uint8_t  battery_pct;     /* Battery percentage (0-100) */
} sleep_data_payload_t;

/* ---- Environment Data Payload (10 bytes, Climate → Hub every 30s) ---- */
typedef struct __attribute__((packed)) {
    int16_t  temperature;     /* °C ×100, e.g. 2067 = 20.67°C */
    uint16_t humidity;        /* RH% ×100, e.g. 4520 = 45.20% */
    uint16_t co2_ppm;         /* CO2 concentration in ppm */
    uint8_t  hvac_state;      /* Bitfield: bit0=cooling, bit1=heating, bit2=fan */
    uint8_t  heater_state;    /* 0=off, 1=on, 2=triac_dimming */
    uint8_t  humidifier_state;/* 0=off, 1=humidify, 2=dehumidify */
    uint8_t  errors;          /* Error bitfield: bit0=sensor, bit1=relay, bit2=comm */
} env_data_payload_t;

/* ---- Shade Status Payload (11 bytes, Shade → Hub every 60s) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  position;        /* 0-100%, 0=closed, 100=open */
    uint16_t ambient_light;   /* Lux from VEML7700 */
    uint8_t  led_warm;        /* Warm white LED 0-255 */
    uint8_t  led_amber;       /* Amber LED 0-255 */
    uint8_t  led_cool;        /* Cool white LED 0-255 */
    uint32_t dawn_time;       /* Next dawn start (Unix timestamp) */
    uint8_t  errors;          /* Error bitfield: bit0=motor, bit1=limit, bit2=led */
} shade_status_payload_t;

/* ---- Hub Command Payload (variable, Hub → nodes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  target_id;       /* Target node ID */
    uint8_t  cmd_type;        /* Command type */
    uint8_t  param_len;       /* Length of following parameters */
    uint8_t  params[24];      /* Command-specific parameters */
} hub_command_payload_t;

/* ---- Command Types ---- */
#define CMD_CLIMATE_SETPOINT   0x01  /* params: [temp_i16(2), hum_u16(2)] */
#define CMD_CLIMATE_HVAC       0x02  /* params: [mode(1): 0=off, 1=cool, 2=heat, 3=auto] */
#define CMD_SHADE_POSITION     0x03  /* params: [position(1): 0-100%] */
#define CMD_SHADE_DAWN         0x04  /* params: [dawn_time_u32(4)] */
#define CMD_ALARM_SET          0x05  /* params: [window_start_u32(4), window_end_u32(4)] */
#define CMD_ALARM_CANCEL       0x06  /* params: none */
#define CMD_SOUND_SET          0x07  /* params: [sound_id(1), volume(1)] */
#define CMD_OTA_TRIGGER        0x08  /* params: [target_id(1), size_u32(4), crc_u16(2)] */
#define CMD_CALIBRATE          0x09  /* params: [sensor_id(1), value(4)] */

/* ---- Alarm Types ---- */
#define ALARM_MORNING          0x01  /* Smart alarm (light+sound+shade) */
#define ALARM_SNOOZE           0x02  /* Snooze (sound off, 5 min delay) */
#define ALARM_EMERGENCY        0x03  /* Emergency (loud sound, all lights on) */

/* ---- Sleep Stages ---- */
#define STAGE_AWAKE            0
#define STAGE_LIGHT            1
#define STAGE_DEEP             2
#define STAGE_REM              3

/* ---- BLE GATT Service UUIDs ---- */
#define BLE_UUID_SLEEPSYNC_SERVICE  0xFFC0
#define BLE_UUID_SLEEP_SCORE       0xFFC0
#define BLE_UUID_ENVIRONMENT       0xFFC1
#define BLE_UUID_SOUND_CONTROL     0xFFC2
#define BLE_UUID_CLIMATE_CONTROL   0xFFC3
#define BLE_UUID_SHADE_CONTROL     0xFFC4
#define BLE_UUID_ALARM_CONFIG      0xFFC5
#define BLE_UUID_SYSTEM_STATUS     0xFFC6
#define BLE_UUID_WIFI_CONFIG       0xFFC7

/* ---- CRC-16/CCITT ---- */
uint16_t mesh_crc16(const uint8_t *data, uint16_t len);

/* ---- Build a mesh message ---- */
uint16_t mesh_build_message(uint8_t src, uint8_t dst, uint8_t type,
                             const uint8_t *payload, uint8_t payload_len,
                             uint8_t *out, uint16_t out_max);

/* ---- Parse and validate a received message ---- */
int8_t mesh_parse_message(const uint8_t *raw, uint16_t raw_len,
                           uint8_t *src, uint8_t *dst, uint8_t *type,
                           uint8_t *payload, uint8_t *payload_len);