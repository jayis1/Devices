/*
 * mesh_protocol.h — Shared Sub-GHz LoRa mesh protocol definitions
 *
 * Used by all PestSentinel field nodes (hub, trap, field-sensor, dispenser)
 * Scout cameras use Wi-Fi and are not on this mesh.
 *
 * TDMA: 16 slots × 200ms = 3.2s frame (scalable to many field nodes)
 * Adaptive SF: SF7 (close), SF9 (mid), SF12 (far / alert)
 *
 * SAFETY NOTE: Disease infection-window + outbreak alerts use SF12
 * broadcast and override normal TDMA scheduling.
 */

#pragma once

#include <stdint.h>

/* ---- Node IDs ---- */
#define NODE_ID_HUB          0x00
#define NODE_ID_BROADCAST    0xFF
/* Field node IDs assigned dynamically at join (0x10-0xEF) */
#define NODE_ID_RANGE_MIN    0x10
#define NODE_ID_RANGE_MAX    0xEF

/* ---- Node types ---- */
#define NODE_TYPE_HUB          0x00
#define NODE_TYPE_SCOUT_CAM    0x01   /* not on LoRa mesh — WiFi only */
#define NODE_TYPE_TRAP         0x02
#define NODE_TYPE_FIELD_SENSOR 0x03
#define NODE_TYPE_DISPENSER    0x04

/* ---- TDMA timing ---- */
#define TDMA_FRAME_MS       3200
#define TDMA_SLOT_MS        200
#define TDMA_NUM_SLOTS      16
#define TDMA_GUARD_US       2000   /* guard time between slots */
/* Slot 0 = hub beacon/cmd, slots 1-15 = node data (assigned by hub) */

/* ---- Spreading factors (adaptive per link) ---- */
#define SF_NORMAL   7    /* close nodes, high data rate */
#define SF_MID      9    /* mid-range */
#define SF_FAR      11   /* far nodes, low data rate */
#define SF_ALERT    12   /* outbreak / disease alerts — max range */

/* ---- Packet types ---- */
#define PKT_HUB_BEACON      0x01   /* hub broadcast: slot assignments, time sync */
#define PKT_JOIN_REQ        0x02   /* node → hub: request to join mesh */
#define PKT_JOIN_ACK        0x03   /* hub → node: assigned node ID + slot */
#define PKT_TRAP_DATA       0x04   /* trap counts + wingbeat species histogram */
#define PKT_SENSOR_DATA     0x05   /* field microclimate data */
#define PKT_DISPENSER_STATE 0x06   /* dispenser status + reservoir level */
#define PKT_DISPATCH_CMD    0x07   /* hub → dispenser: activate intervention */
#define PKT_ACK             0x08
#define PKT_OTA_BLOCK       0x09
#define PKT_CALIBRATION     0x0A
#define PKT_OUTBREAK_ALERT  0x0B   /* SF12 broadcast: predicted outbreak */
#define PKT_DISEASE_ALERT   0x0C   /* SF12 broadcast: infection window open */
#define PKT_HEARTBEAT       0x0D
#define PKT_WIND_ALERT      0x0E   /* field sensor: wind too high for spray */
#define PKT_PHEROMONE_CTRL  0x0F   /* hub → dispenser: pheromone heater duty */
#define PKT_LURE_REPLACE    0x10   /* hub → trap: lure depleted, replace */

/* ---- Sync word ---- */
#define MESH_SYNC_WORD      0x5053  /* "PS" */

/* Max payload size (fits in LoRa SF7 125kHz) */
#define MESH_MAX_PAYLOAD    64

/* ---- Packet structure (over the air) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  preamble[4];   /* 0xAA 0xAA 0xAA 0xAA */
    uint16_t sync;          /* MESH_SYNC_WORD */
    uint8_t  len;           /* payload length */
    uint8_t  src_id;        /* source node ID */
    uint8_t  dst_id;        /* destination node ID (0xFF=broadcast) */
    uint8_t  pkt_type;      /* PKT_* type */
    uint8_t  sf_used;       /* spreading factor used for this packet */
    int8_t   snr_db;        /* SNR reported by receiver (filled on RX) */
    int8_t   rssi_dbm;      /* RSSI reported by receiver (filled on RX) */
    uint8_t  payload[MESH_MAX_PAYLOAD];
    uint16_t crc16;         /* CRC-16/CCITT over len+src+dst+type+sf+payload */
} mesh_packet_t;

/* ---- Trap data payload (52 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  trap_type;        /* 0=delta, 1=bucket, 2=sticky-board, 3=light */
    uint8_t  lure_species_id;  /* pheromone lure target species (0=none) */
    uint8_t  lure_pct_remaining; /* estimated lure remaining (0-100) */
    uint16_t total_count_24h;  /* total insects trapped in last 24h */
    uint16_t count_this_hour;  /* count in the current hour */
    uint8_t  species_count;    /* number of species in histogram below */
    /* species histogram: species_id + count pairs (up to 12 species) */
    uint8_t  species_ids[12];
    uint16_t species_counts[12];
    int16_t  temp_c_x10;       /* trap-local temperature ×10 */
    uint16_t humidity_x10;     /* trap-local RH ×10 */
    uint8_t  battery_pct;      /* 0-100 */
    uint8_t  flags;            /* bit0=lure_low, bit1=board_full, bit2=camera_fault */
} trap_data_payload_t;

/* ---- Field sensor data payload (40 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint16_t leaf_wetness_raw;   /* Davis 6420 raw ADC (0=dry, ~700=wet) */
    uint8_t  leaf_wet_pct;       /* 0-100 computed wetness */
    uint16_t leaf_wet_minutes;   /* continuous wetness duration (minutes) */
    uint16_t soil_moisture_raw;  /* capacitive probe raw */
    uint8_t  soil_moisture_pct;  /* 0-100 computed */
    int16_t  canopy_temp_c_x10;  /* canopy air temp ×10 */
    uint16_t canopy_rh_x10;      /* canopy RH ×10 */
    uint16_t wind_speed_ms_x10;  /* wind speed m/s ×10 (avg since last report) */
    uint16_t wind_gust_ms_x10;   /* peak gust m/s ×10 */
    uint16_t wind_dir_deg;       /* 0-359 degrees */
    uint16_t rain_ticks;         /* tipping-bucket clicks (0.2mm each) since last */
    uint16_t rain_mm_x10;        /* rainfall mm ×10 */
    int16_t  pressure_hpa_x10;   /* barometric pressure hPa ×10 */
    uint16_t degree_days_x100;   /* accumulated degree-days ×100 (on-device) */
    uint8_t  battery_pct;
    uint8_t  flags;              /* bit0=wind_high, bit1=rain_event, bit2=sensor_fault */
} sensor_data_payload_t;

/* ---- Dispenser state payload (24 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  state;              /* 0=idle, 1=ultrasonic, 2=pheromone, 3=spray, 4=fault */
    uint8_t  active_agent;       /* 0=none, 1=ultrasonic, 2=pheromone-A, 3=pheromone-B, 4=kaolin, 5=neem, 6=Bt, 7=nematodes */
    uint16_t reservoir_ml;       /* remaining liquid (spray agents) */
    uint16_t pheromone_pct;      /* pheromone pad remaining (0-100) */
    uint8_t  last_dispatch_id;   /* dispatch command ID last executed */
    uint32_t active_seconds;     /* seconds of active dispensing in current session */
    uint16_t total_doses_today;  /* total doses dispensed today */
    uint8_t  target_species_id;  /* species currently targeted */
    uint8_t  battery_pct;
    uint8_t  flags;              /* bit0=reservoir_low, bit1=pad_low, bit2=motor_fault, bit3=night_mode */
} dispenser_state_payload_t;

/* ---- Dispatch command payload (16 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  dispatch_id;        /* unique ID for this dispatch */
    uint8_t  mode;               /* 1=ultrasonic, 2=pheromone, 3=spray */
    uint8_t  agent_id;           /* which agent (see active_agent above) */
    uint8_t  target_species_id;  /* pest species to target */
    uint16_t dose_ml;            /* spray volume (mode=3) */
    uint16_t duration_s;         /* ultrasonic burst duration (mode=1) or pheromone session (mode=2) */
    uint16_t frequency_hz;       /* ultrasonic center frequency (mode=1) */
    uint8_t  duty_cycle_pct;     /* ultrasonic duty cycle / pheromone heater duty */
    uint8_t  repeat_count;       /* number of repetitions */
    uint16_t interval_s;         /* interval between repetitions */
    uint16_t wind_limit_ms_x10;  /* max wind speed for spray (safety gate) */
    uint8_t  reserved;
} dispatch_cmd_payload_t;

/* ---- Outbreak alert payload (20 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  species_id;         /* pest species predicted to outbreak */
    uint8_t  zone_id;            /* affected zone */
    uint8_t  severity;           /* 0-100 risk score */
    uint16_t predicted_eil;      /* predicted peak population (economic injury level units) */
    uint16_t current_count;      /* current trap count / detection count */
    uint16_t days_to_threshold;  /* days until predicted EIL crossing */
    uint16_t recommended_action; /* bitmask: 1=ultrasonic,2=pheromone,4=spray,8=manual_scout */
    uint8_t  disease_id;         /* if disease outbreak (0=pest, nonzero=disease) */
    uint8_t  infection_window_h; /* disease: infection window duration in hours */
    uint16_t degree_days;        /* current degree-day accumulation */
    uint32_t biofix_epoch;       /* biofix date (unix epoch) */
    uint8_t  confidence;         /* model confidence 0-100 */
    uint8_t  reserved;
} outbreak_alert_payload_t;

/* ---- Hub beacon payload (16 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint32_t epoch_time;         /* hub time for node sync */
    uint8_t  num_slots_used;     /* currently assigned TDMA slots */
    uint8_t  sf_default;         /* default SF for new nodes */
    uint8_t  weather_valid;      /* 1 if hub has fresh weather data */
    uint16_t wind_safe_ms_x10;   /* current wind-safe threshold for spray */
    uint8_t  outbreak_active;    /* 1 if any outbreak alert currently active */
    uint8_t  disease_active;     /* 1 if any disease alert currently active */
    uint16_t ota_version;        /* latest firmware version (0=no OTA) */
    uint32_t flags;              /* system flags */
} hub_beacon_payload_t;

/* ---- Join request payload (8 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  node_type;          /* NODE_TYPE_* */
    uint8_t  hw_version;
    uint16_t fw_version;
    uint8_t  capabilities;       /* bitmask of node capabilities */
    uint8_t  battery_pct;
    uint16_t serial_number;
} join_req_payload_t;

/* ---- CRC-16/CCITT ---- */
uint16_t mesh_crc16(const uint8_t *data, uint8_t len);

/* ---- Packet helpers ---- */
void mesh_packet_init(mesh_packet_t *pkt, uint8_t src_id, uint8_t dst_id,
                      uint8_t pkt_type, uint8_t sf);
uint8_t mesh_packet_serialize(const mesh_packet_t *pkt, uint8_t *out, uint8_t max_len);
int mesh_packet_parse(const uint8_t *raw, uint8_t raw_len, mesh_packet_t *pkt);