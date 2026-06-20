# GreenPulse Mesh Protocol Specification

## Overview

GreenPulse uses a compact binary protocol over a Sub-GHz (868/915 MHz) mesh network via SX1262. All payloads are CRC16-CCITT protected for integrity. Plant tags relay neighbor packets (store-and-forward mesh) to extend range.

## Frame Format

```
┌──────────┬──────────┬──────────┬───────────────┬──────────┐
│ Type (1) │ Node (1) │ Seq (1)  │ Payload (N)   │ CRC16 (2)│
└──────────┴──────────┴──────────┴───────────────┴──────────┘
```

- **Type:** Message type (see `gp_msg_type_t`)
- **Node:** Source node ID (see `GP_NODE_ID_*`)
- **Seq:** Sequence number (wraps at 255)
- **Payload:** Type-specific struct (packed, little-endian)
- **CRC16:** CRC16-CCITT over Type + Node + Seq + Payload

## Node IDs

| ID | Node |
|----|------|
| 0x00 | Hub |
| 0x01-0x3F | Plant Tags (up to 63) |
| 0x40 | Leaf Scanner |
| 0x50-0x57 | Water Valves (up to 8 zones) |

## Message Types

### GP_MSG_TELEMETRY (0x10) — Plant Tag → Hub

Sent every 15 minutes. Contains soil moisture, light, temp, humidity, battery.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x10
    uint8_t  node_id;          // tag ID
    uint8_t  seq;
    uint8_t  flags;            // alert bitmask
    uint8_t  soil_moisture;    // volumetric water content % (0-100)
    uint16_t ambient_lux;      // lux * 10
    int16_t  temp_centic;      // temp centi-degC
    uint16_t humidity_centi;   // RH * 100 (%)
    uint8_t  battery_pct;      // 0-100
    uint8_t  plant_profile_id; // species profile index
    uint16_t crc16;
} gp_telemetry_payload_t;  // 14 bytes
```

### GP_MSG_WATERING_CMD (0x20) — Hub → Valve

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x20
    uint8_t  node_id;          // 0x00 (hub)
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  zone;             // valve zone (0-7)
    uint8_t  emitter_id;      // drip emitter (0=zone, 1-N=per plant)
    uint16_t duration_s;       // watering duration (seconds)
    uint16_t target_ml;        // target volume (ml), 0 = duration only
    uint16_t crc16;
} gp_watering_cmd_payload_t;  // 11 bytes
```

### GP_MSG_WATERING_ACK (0x21) — Valve → Hub

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x21
    uint8_t  node_id;          // 0x50 | zone
    uint8_t  seq;
    uint8_t  flags;            // GP_ALERT_LEAK if flow after close
    uint8_t  status;           // GP_WATER_* (0=ok 1=no_flow 2=leak 3=timeout)
    uint16_t ml_delivered;    // actual liters delivered (ml)
    uint16_t duration_s;       // actual duration
    uint16_t crc16;
} gp_watering_ack_payload_t;  // 10 bytes
```

### GP_MSG_SCAN_RESULT (0x30) — Scanner → Hub/Cloud

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x30
    uint8_t  node_id;          // 0x40
    uint8_t  seq;
    uint8_t  flags;            // DISEASE_SUSPECT / PEST_DETECTED
    uint8_t  plant_tag_id;    // which tag (0 = unpaired)
    uint8_t  species_id_lo;   // species ID low byte
    uint8_t  species_id_hi;   // species ID high byte
    uint8_t  species_conf;    // 0-100
    uint8_t  disease_class;   // 0=healthy 1=mildew 2=spot 3=rust 4=rot 5=pest
    uint8_t  disease_conf;    // 0-100
    uint8_t  pest_count;      // detected pest count
    uint16_t crc16;
} gp_scan_result_payload_t;  // 13 bytes
```

### GP_MSG_STRESS_SCORE (0x50) — Hub → Mesh Broadcast

Broadcast every 15 min after risk inference. Valve + mobile use this.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x50
    uint8_t  node_id;          // 0x00 (hub)
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  plant_tag_id;    // which plant
    uint8_t  disease_risk;    // 0-100 (3-day forecast)
    uint8_t  water_risk;      // 0-100 (wilt risk if not watered)
    uint8_t  light_risk;      // 0-100 (light deficiency risk)
    uint8_t  status;          // GP_PLANT_*
    uint16_t hours_to_water;  // hours until watering needed (0xFFFF = N/A)
    uint16_t crc16;
} gp_stress_score_payload_t;  // 14 bytes
```

## Alert Flags

| Flag | Value | Condition |
|------|-------|-----------|
| LOW_MOISTURE | 0x01 | Soil below species threshold |
| LOW_LIGHT | 0x02 | DLI below species minimum |
| HIGH_TEMP | 0x04 | Temp above species max |
| LOW_TEMP | 0x08 | Temp below species min |
| DISEASE_SUSPECT | 0x10 | Scanner flagged leaf as suspect |
| PEST_DETECTED | 0x20 | Scanner detected pests |
| LOW_BATT | 0x40 | Tag battery < 15% |
| LEAK | 0x80 | Valve: flow after close |

## Plant Status Codes

| Status | Value | Description |
|--------|-------|-------------|
| OK | 0 | All good |
| WATER_SOON | 1 | Moisture declining, water in <48h |
| WATER_NOW | 2 | Moisture at threshold, water now |
| LOW_LIGHT | 3 | Needs more light |
| DISEASE | 4 | Disease/pest detected by scanner |
| STRESS | 5 | Temp/light out of species range |

## Watering Status Codes

| Status | Value | Description |
|--------|-------|-------------|
| OK | 0 | Watering successful |
| NO_FLOW | 1 | Empty reservoir or blocked line |
| LEAK | 2 | Flow detected after valve close |
| TIMEOUT | 3 | Max duration exceeded (safety) |

## Plant Care Profiles

Built-in species care profiles (edge-resident on hub). Full 4,000-species DB in cloud.

| Profile ID | Species | Min Moisture % | Max Moisture % | Light (lux·hr/10) | Temp Min | Temp Max | Humidity % | Water Interval (h) |
|-----------|---------|---------------|---------------|-------------------|----------|----------|-----------|-------------------|
| 1 | Monstera | 35 | 80 | 3000 | 15°C | 35°C | 40 | 168 |
| 2 | Calathea | 50 | 85 | 1500 | 16°C | 30°C | 60 | 96 |
| 3 | Fiddle Leaf | 30 | 75 | 5000 | 12°C | 30°C | 30 | 168 |
| 4 | Snake Plant | 15 | 60 | 800 | 10°C | 35°C | 20 | 360 |
| 5 | Pothos | 30 | 80 | 1200 | 15°C | 32°C | 30 | 168 |
| 6 | Cactus | 10 | 40 | 4000 | 5°C | 40°C | 10 | 504 |
| 7 | Fern | 55 | 90 | 1500 | 15°C | 28°C | 60 | 72 |
| 8 | Orchid | 40 | 75 | 3000 | 15°C | 30°C | 50 | 168 |
| ... | ... | ... | ... | ... | ... | ... | ... | ... |