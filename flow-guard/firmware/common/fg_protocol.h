/**
 * FlowGuard - Common Protocol Definitions
 * Shared Zigbee cluster definitions, packet structures, and constants
 * Used by all nodes (hub, valve controller, pipe sensor, appliance monitor)
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#ifndef FG_PROTOCOL_H
#define FG_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * System Constants
 * ============================================================ */

#define FG_VERSION_MAJOR        1
#define FG_VERSION_MINOR        0
#define FG_VERSION_PATCH        0

/* Zigbee network parameters */
#define FG_ZIGBEE_CHANNEL       25          /* 2.4GHz channel 25 (less WiFi interference) */
#define FG_ZIGBEE_PAN_ID       0xF60D      /* FlowGuard PAN ID */
#define FG_ZIGBEE_MAX_NODES    16          /* Max nodes per hub (1 valve + 8 pipe + 4 appliance + 3 spare) */

/* Node types */
typedef enum {
    FG_NODE_HUB               = 0x00,
    FG_NODE_VALVE_CTRL        = 0x01,
    FG_NODE_PIPE_SENSOR       = 0x02,
    FG_NODE_APPLIANCE_MONITOR = 0x03,
} fg_node_type_t;

/* Node IDs (assigned during commissioning) */
#define FG_NODE_ID_HUB          0x0000
#define FG_NODE_ID_VALVE_BASE   0x0001
#define FG_NODE_ID_PIPE_BASE    0x0010
#define FG_NODE_ID_APPLIANCE_BASE 0x0020

/* ============================================================
 * Custom Zigbee Cluster IDs
 * ============================================================ */

#define FG_CLUSTER_CONTROL      0xFC00   /* FlowGuard Control cluster */
#define FG_CLUSTER_COMMAND      0xFC01   /* FlowGuard Command cluster */
#define FG_CLUSTER_ALERT        0xFC02   /* FlowGuard Alert cluster */

/* ============================================================
 * FlowGuard Control Cluster Attributes (0xFC00)
 * ============================================================ */

typedef enum {
    FG_ATTR_VALVE_STATE       = 0x0000,  /* enum8: valve state */
    FG_ATTR_FLOW_RATE         = 0x0001,  /* uint16: mL/min */
    FG_ATTR_PRESSURE          = 0x0002,  /* uint16: kPa × 10 */
    FG_ATTR_TEMPERATURE       = 0x0003,  /* int16: °C × 100 */
    FG_ATTR_LEAK_STATE        = 0x0004,  /* enum8: leak detection state */
    FG_ATTR_VIBRATION_RMS    = 0x0005,  /* uint16: mg RMS × 10 */
    FG_ATTR_ACOUSTIC_ANOMALY = 0x0006,  /* uint8: score 0-255 */
} fg_control_attr_t;

/* Valve state enum */
typedef enum {
    FG_VALVE_OPEN       = 0x00,
    FG_VALVE_CLOSED     = 0x01,
    FG_VALVE_CLOSING    = 0x02,
    FG_VALVE_OPENING    = 0x03,
    FG_VALVE_ERROR      = 0x04,
    FG_VALVE_UNKNOWN    = 0xFF,
} fg_valve_state_t;

/* Leak state enum */
typedef enum {
    FG_LEAK_DRY         = 0x00,
    FG_LEAK_WET         = 0x01,
    FG_LEAK_ALERT       = 0x02,
    FG_LEAK_CONFIRMED   = 0x03,
} fg_leak_state_t;

/* ============================================================
 * FlowGuard Command Cluster (0xFC01)
 * ============================================================ */

typedef enum {
    FG_CMD_VALVE_OPEN           = 0x00,  /* payload: auth_token[4], reason[1] */
    FG_CMD_VALVE_CLOSE          = 0x01,  /* payload: auth_token[4], reason[1] */
    FG_CMD_START_ACOUSTIC_CAPTURE = 0x02, /* payload: duration_sec[1] */
    FG_CMD_SET_SAMPLING_RATE    = 0x03,  /* payload: sensor_mask[2], rate_hz[2] */
    FG_CMD_EMERGENCY_SHUTDOWN   = 0x04,  /* payload: auth_token[4], source[1] */
    FG_CMD_RESET_NODE           = 0x05,  /* payload: node_id[2] */
} fg_command_t;

/* Valve command reasons */
typedef enum {
    FG_REASON_USER_MANUAL      = 0x00,
    FG_REASON_LEAK_DETECTED    = 0x01,
    FG_REASON_PRESSURE_ANOMALY = 0x02,
    FG_REASON_FREEZE_RISK      = 0x03,
    FG_REASON_SCHEDULED        = 0x04,
    FG_REASON_REMOTE_APP       = 0x05,
    FG_REASON_EMERGENCY        = 0x06,
    FG_REASON_APPLIANCE_FAULT  = 0x07,
} fg_valve_reason_t;

/* Emergency shutdown sources */
typedef enum {
    FG_SOURCE_HUB              = 0x00,
    FG_SOURCE_VALVE_CONTROLLER = 0x01,
    FG_SOURCE_PIPE_SENSOR      = 0x02,
    FG_SOURCE_CLOUD            = 0x03,
    FG_SOURCE_MOBILE_APP       = 0x04,
} fg_emergency_source_t;

/* ============================================================
 * FlowGuard Alert Cluster (0xFC02)
 * ============================================================ */

typedef enum {
    FG_ALERT_INFO       = 0x00,
    FG_ALERT_WARNING    = 0x01,
    FG_ALERT_CRITICAL   = 0x02,
    FG_ALERT_EMERGENCY  = 0x03,
} fg_alert_level_t;

typedef enum {
    FG_ALERT_TYPE_LEAK      = 0x00,
    FG_ALERT_TYPE_PRESSURE  = 0x01,
    FG_ALERT_TYPE_FREEZE    = 0x02,
    FG_ALERT_TYPE_HAMMER    = 0x03,
    FG_ALERT_TYPE_APPLIANCE = 0x04,
    FG_ALERT_TYPE_BATTERY   = 0x05,
    FG_ALERT_TYPE_FLOW      = 0x06,
} fg_alert_type_t;

typedef struct {
    fg_alert_level_t level;
    fg_alert_type_t  type;
    uint16_t         source_node_id;
    char             message[64];
} fg_alert_t;

/* ============================================================
 * Sensor Data Structures
 * ============================================================ */

/* Pipe sensor full report (transmitted every 5 minutes or on anomaly) */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    int16_t  temperature_cx100;    /* °C × 100 (e.g., 2345 = 23.45°C) */
    uint16_t humidity_cx10;         /* %RH × 10 (e.g., 552 = 55.2%) */
    uint16_t vibration_rms_mgx10;   /* mg RMS × 10 */
    uint8_t  acoustic_anomaly;      /* 0-255 score */
    uint8_t  leak_state;            /* fg_leak_state_t */
    uint16_t battery_mv;            /* Battery voltage in mV */
    uint32_t uptime_sec;            /* Node uptime in seconds */
} fg_pipe_sensor_report_t;

/* Appliance monitor report (transmitted on flow start/stop and periodically) */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint16_t flow_rate_ml_min;     /* mL/min instantaneous */
    uint32_t flow_volume_ml;       /* Cumulative volume in mL */
    int16_t  temperature_cx100;    /* Water temperature °C × 100 */
    uint16_t humidity_cx10;         /* Ambient humidity %RH × 10 */
    int16_t  pressure_kpa_x10;    /* Pressure kPa × 10 */
    uint8_t  leak_probe_1;          /* Conductivity probe 1: 0=dry, 1=wet */
    uint8_t  leak_probe_2;          /* Conductivity probe 2: 0=dry, 1=wet */
    uint16_t battery_mv;            /* Battery voltage in mV */
} fg_appliance_report_t;

/* Valve controller status report (transmitted every 30 seconds) */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint8_t  valve_state;           /* fg_valve_state_t */
    uint16_t flow_rate_ml_min;      /* Whole-home flow rate */
    int16_t  pressure_kpa_x10;     /* Whole-home pressure */
    int16_t  temperature_cx100;     /* Pipe temperature */
    uint16_t motor_current_ma;      /* Motor current during last operation */
    uint8_t  heater_state;          /* 0=off, 1=on */
    uint16_t battery_mv;            /* Backup battery voltage */
    uint8_t  supply_12v_ok;         /* 12V supply present: 0=no, 1=yes */
    uint32_t cumulative_gallons_x10;/* Cumulative water usage × 10 */
} fg_valve_status_t;

/* ============================================================
 * Authentication
 * ============================================================ */

/* 4-byte auth token for valve commands (prevents accidental Zigbee spoofing) */
#define FG_AUTH_TOKEN_LEN  4

/* Default auth token (changed during pairing) */
#define FG_AUTH_TOKEN_DEFAULT  { 0xF6, 0x0D, 0x00, 0x01 }

/* ============================================================
 * Timing Constants
 * ============================================================ */

#define FG_REPORT_INTERVAL_SEC      300     /* Normal report interval: 5 minutes */
#define FG_REPORT_INTERVAL_FAST_SEC 10      /* Fast report during anomaly: 10 seconds */
#define FG_VALVE_OPERATION_TIME_MS  5000    /* Valve open/close operation time: 5 seconds */
#define FG_VALVE_EMERGENCY_TIMEOUT_MS 3000  /* Emergency valve close timeout: 3 seconds */
#define FG_HUB_UNREACHABLE_SEC     60      /* If hub unreachable for 60s, valve auto-closes on leak */
#define FG_LEAK_DEBOUNCE_MS        300      /* Leak detection debounce: 300ms */
#define FG_BATTERY_LOW_MV          2400    /* Low battery threshold: 2.4V (AA) or 2.1V (coin cell) */
#define FG_BATTERY_CRITICAL_MV     2000    /* Critical battery: 2.0V (AA) or 1.7V (coin cell) */

/* ============================================================
 * ML Model Constants (TFLite Micro)
 * ============================================================ */

#define FG_LEAK_MODEL_INPUT_SIZE   96000   /* 2 sec × 48kHz × 1 channel × 16-bit / 2 = 19200 samples */
#define FG_LEAK_MODEL_SIZE         85000   /* INT8 quantized model size in bytes */
#define FG_LEAK_NUM_CLASSES        6       /* normal, flow, leak, hammer, air, cavitation */
#define FG_LEAK_THRESHOLD_WARN     0.6f    /* Warning threshold */
#define FG_LEAK_THRESHOLD_CRITICAL 0.85f   /* Critical threshold (auto-shutoff) */

/* Acoustic classification labels */
typedef enum {
    FG_ACOUSTIC_NORMAL     = 0,
    FG_ACOUSTIC_FLOW       = 1,
    FG_ACOUSTIC_LEAK       = 2,
    FG_ACOUSTIC_HAMMER     = 3,
    FG_ACOUSTIC_AIR        = 4,
    FG_ACOUSTIC_CAVITATION = 5,
} fg_acoustic_class_t;

/* ============================================================
 * CRC and Checksum
 * ============================================================ */

uint16_t fg_crc16(const uint8_t *data, uint16_t len);

/* Zigbee APS layer encryption uses AES-128-CCM (handled by nRF52 hardware) */

#endif /* FG_PROTOCOL_H */