/*
 * mesh_protocol.h — Shared Sub-GHz mesh protocol definitions
 *
 * Used by all WashWise nodes (hub, washer, dryer, scanner)
 * TDMA: 5 slots × 100ms = 500ms frame
 *
 * SAFETY NOTE: Dryer node fire alerts override normal TDMA.
 */

#pragma once

#include <stdint.h>

/* ---- Node IDs ---- */
#define NODE_ID_HUB        0x00
#define NODE_ID_BROADCAST  0xFF
#define NODE_ID_WASHER     0x01
#define NODE_ID_DRYER      0x02
#define NODE_ID_SCANNER    0x03

/* ---- TDMA timing ---- */
#define TDMA_FRAME_MS       500
#define TDMA_SLOT_MS        100
#define TDMA_NUM_SLOTS      5
#define TDMA_GUARD_US       500   /* guard time between slots */

/* ---- Packet types ---- */
#define PKT_WASHER_DATA     0x01
#define PKT_DRYER_DATA      0x02
#define PKT_SCAN_RESULT     0x03
#define PKT_COMMAND         0x04
#define PKT_ACK             0x05
#define PKT_OTA_BLOCK       0x06
#define PKT_CALIBRATION     0x07
#define PKT_FIRE_ALERT      0x08  /* highest priority — SF10 broadcast */
#define PKT_ENERGY_DATA     0x09
#define PKT_HEARTBEAT       0x0A

/* ---- Sync word ---- */
#define MESH_SYNC_WORD      0x5A5A  /* "WW" */

/* Max payload size (fits in LoRa SF7 125kHz) */
#define MESH_MAX_PAYLOAD    50

/* ---- Packet structure (over the air) ---- */
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

/* ---- Washer data payload (36 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  cycle_phase;     /* 0=idle,1=fill,2=wash,3=rinse,4=spin,5=done */
    uint16_t vibration_rms_x10; /* vibration RMS in milli-g ×10 */
    uint16_t flow_rate_mlmin;  /* current water flow mL/min */
    uint16_t total_water_ml;   /* total water this cycle */
    int16_t  water_temp_c_x10; /* wash water temp ×10 °C */
    uint16_t ambient_hum_x10;  /* ambient humidity ×10 % */
    uint8_t  motor_state;      /* 0=off,1=on,2=spin */
    uint16_t current_ma;       /* washer power draw mA */
    uint16_t detergent_mg;    /* detergent dispensed this cycle mg */
    uint16_t reservoir_g_x10;  /* detergent reservoir weight ×10 g */
    uint8_t  fabric_type;      /* 0=unknown,1=cotton,2=poly,3=wool,4=silk,5=denim,6=blend */
    uint8_t  imbalance_flag;   /* 0=ok,1=warning,2=severe */
    uint8_t  leak_flag;        /* 0=ok,1=suspected,2=confirmed */
    uint8_t  battery_pct;      /* 0-100 */
    uint8_t  signal_rssi;      /* RSSI dB above -100 */
    uint16_t reserved;         /* future use */
} washer_data_payload_t;

/* ---- Dryer data payload (30 bytes) ---- */
typedef struct __attribute__((packed)) {
    int16_t  exhaust_temp_c_x10;  /* exhaust temp ×10 °C */
    int16_t  ambient_temp_c_x10;  /* ambient room temp ×10 °C */
    uint16_t diff_pressure_pa;    /* differential pressure Pa (lint clog) */
    uint16_t exhaust_hum_x10;     /* exhaust humidity ×10 % */
    uint16_t vibration_rms_x10;   /* vibration RMS milli-g ×10 */
    uint16_t current_ma;          /* dryer power draw mA */
    uint8_t  dryer_state;         /* 0=off,1=heating,2=tumbling,3=cooling,4=done */
    uint8_t  heating_on;          /* 0=off,1=on (heating element) */
    uint8_t  fire_risk_score;     /* 0-255 (0.0-1.0) ML prediction */
    uint8_t  lint_clog_level;     /* 0=clean,1=mild,2=moderate,3=severe */
    uint8_t  dryness_level;       /* 0=wet,1=damp,2=dry,3=over-dry */
    uint8_t  alert_level;         /* 0=ok,1=info,2=warning,3=critical,4=emergency */
    uint8_t  battery_pct;        /* 0-100 */
    uint8_t  signal_rssi;         /* RSSI */
    uint16_t reserved;            /* future use */
} dryer_data_payload_t;

/* ---- Scan result payload (40 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  fabric_type;     /* 0=unknown,1=cotton,2=polyester,3=wool,4=silk,
                                 5=denim,6=nylon,7=linen,8=blend */
    uint8_t  fabric_conf;     /* 0-255 confidence */
    uint8_t  stain_type;      /* 0=clean,1=coffee,2=wine,3=blood,4=grease,
                                 5=grass,6=ink,7=food,8=sweat,9=rust,10=unknown */
    uint8_t  stain_conf;      /* 0-255 confidence */
    int16_t  wash_temp_c_x10; /* recommended wash temp ×10 °C */
    uint8_t  recommended_cycle; /* 0=normal,1=delicate,2=heavy,3=quick,4=handwash */
    uint8_t  detergent_ml;    /* recommended detergent mL */
    uint8_t  pre_treat_id;    /* pre-treatment method ID */
    uint8_t  care_label[8];   /* care symbols (extracted) */
    uint16_t image_id;        /* reference to cloud image */
    uint8_t  battery_pct;
    uint8_t  signal_rssi;
    uint16_t reserved;
} scan_result_payload_t;

/* ---- Fire alert payload (12 bytes) — highest priority ---- */
typedef struct __attribute__((packed)) {
    uint8_t  alert_level;       /* 3=critical, 4=emergency */
    uint8_t  fire_risk_score;  /* 0-255 */
    int16_t  exhaust_temp_c_x10;
    uint16_t diff_pressure_pa;
    uint8_t  lint_clog_level;
    uint8_t  heating_on;
    uint16_t timestamp_ms;     /* hub-relative timestamp */
    uint8_t  source_node;
    uint8_t  reserved;
} fire_alert_payload_t;

/* ---- Energy data payload (16 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint16_t cycle_energy_wh;   /* Wh consumed this cycle */
    uint16_t cycle_water_ml;    /* mL water this cycle */
    uint16_t cycle_duration_s;  /* cycle duration seconds */
    uint8_t  node_type;         /* 1=washer, 2=dryer */
    uint8_t  cycle_id;          /* cycle identifier */
    uint16_t estimated_cost_cents; /* estimated cost in cents */
    uint16_t co2_g;             /* estimated CO2 grams */
    uint16_t reserved;
} energy_data_payload_t;

/* ---- Command payload (variable) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;    /* CMD_* */
    uint8_t  param_len;   /* length of following params */
    uint8_t  params[16];  /* command-specific parameters */
} command_payload_t;

/* ---- Command types ---- */
#define CMD_DOSE          0x01  /* params: [detergent_ml(1)] */
#define CMD_CYCLE_SELECT  0x02  /* params: [cycle_type(1), temp_c_x10(2)] */
#define CMD_ALARM_OFF     0x03  /* params: [alarm_mask(1)] */
#define CMD_DRYER_SHUTOFF 0x04  /* params: [advisory_id(1)] */
#define CMD_SCAN          0x05  /* params: [mode(1): 0=standard,1=quick] */
#define CMD_CALIBRATE     0x06  /* params: [sensor_id(1), value(4)] */
#define CMD_FORCE_FAN     0x07  /* params: [duration_s(2)] */
#define CMD_LIGHT_OVERRIDE 0x08 /* params: [r(1),g(1),b(1)] */

/* ---- Alert levels ---- */
#define ALERT_OK        0
#define ALERT_INFO      1
#define ALERT_WARNING   2
#define ALERT_CRITICAL  3
#define ALERT_EMERGENCY 4

/* ---- Cycle phases ---- */
#define PHASE_IDLE      0
#define PHASE_FILL      1
#define PHASE_WASH      2
#define PHASE_RINSE     3
#define PHASE_SPIN      4
#define PHASE_DONE      5

/* ---- CRC-16/CCITT calculation ---- */
uint16_t mesh_crc16(const uint8_t *data, uint16_t len);

/* Build a mesh packet */
uint16_t mesh_build_packet(uint8_t src, uint8_t dst, uint8_t type,
                            const uint8_t *payload, uint8_t payload_len,
                            mesh_packet_t *out);

/* Parse and validate a received packet */
int8_t mesh_parse_packet(const uint8_t *raw, uint16_t raw_len, mesh_packet_t *out);