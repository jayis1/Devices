# PawSync Mesh Protocol Specification

## Overview

PawSync uses a compact binary protocol over a BLE 5.3 mesh network. All payloads are CRC16-CCITT protected for integrity.

## Frame Format

```
┌──────────┬──────────┬──────────┬───────────────┬──────────┐
│ Type (1) │ Node (1) │ Seq (1)  │ Payload (N)   │ CRC16 (2)│
└──────────┴──────────┴──────────┴───────────────┴──────────┘
```

- **Type:** Message type (see `paw_msg_type_t`)
- **Node:** Source node ID (see `PAW_NODE_ID_*`)
- **Seq:** Sequence number (wraps at 255)
- **Payload:** Type-specific struct (packed, little-endian)
- **CRC16:** CRC16-CCITT over Type + Node + Seq + Payload

## Node IDs

| ID | Node |
|----|------|
| 0x00 | Hub |
| 0x01 | Collar Tag |
| 0x02 | Behavior Camera |
| 0x03 | Smart Feeder |

## Message Types

### PAW_MSG_VITALS (0x10) — Collar → Hub

Sent every 60 seconds. Contains the latest HR, HRV, temperature, gait features, and battery.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;            // 0x10
    uint8_t  node_id;         // 0x01
    uint8_t  seq;
    uint8_t  flags;           // alert bitmask
    uint8_t  hr_bpm;          // heart rate (bpm)
    uint16_t hrv_rmssd;       // RMSSD (centi-ms → /100 = ms)
    int16_t  skin_temp_centic;// skin temp (centi-degC → /100 = °C)
    int16_t  gait[6];         // gait features
    uint8_t  battery_pct;     // 0-100
    uint16_t crc16;
} paw_vitals_payload_t;  // 22 bytes
```

### PAW_MSG_ACTIVITY (0x11) — Collar → Hub

Sent every 60 seconds with the current activity classification.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;            // 0x11
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  activity_class; // 0-8 (see table)
    uint8_t  confidence;     // 0-100
    uint16_t duration_s;     // seconds in activity
    int16_t  gait[6];
    uint16_t crc16;
} paw_activity_payload_t;  // 18 bytes
```

### PAW_MSG_BEHAVIOR (0x12) — Camera → Hub/Cloud

Sent every 30 seconds or on anxiety episode.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;            // 0x12
    uint8_t  node_id;         // 0x02
    uint8_t  seq;
    uint8_t  flags;           // PAW_ALERT_ANXIETY if episode
    uint8_t  behavior_class;  // 0-5
    uint8_t  vocalization;    // 0-6
    uint8_t  confidence;
    uint16_t duration_s;
    uint32_t clip_ref;       // SD card clip reference
    uint16_t crc16;
} paw_behavior_payload_t;  // 16 bytes
```

### PAW_MSG_FEEDING (0x13) — Feeder → Hub

Sent on each feeding event.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;            // 0x13
    uint8_t  node_id;         // 0x03
    uint8_t  seq;
    uint8_t  flags;           // PAW_ALERT_APPETITE_LOSS if uneaten
    uint8_t  pet_id;         // RFID pet ID
    uint16_t dispensed_g;
    uint16_t consumed_g;
    uint16_t water_ml;
    uint8_t  hopper_pct;
    uint16_t crc16;
} paw_feeding_payload_t;  // 16 bytes
```

### PAW_MSG_ALERT (0x20) — Any → Hub (mesh flood)

Immediate alert, sent with mesh flood for urgent events.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;            // 0x20
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;           // which alert(s)
    uint16_t value;           // alert-specific value
    uint16_t crc16;
} paw_alert_payload_t;  // 8 bytes
```

### PAW_MSG_WELLNESS (0x30) — Hub → Mesh

Broadcast every 15 minutes with the latest wellness score.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;            // 0x30
    uint8_t  node_id;         // 0x00
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  wellness_score;  // 0-100
    uint8_t  illness_risk;    // 0-100
    uint8_t  anxiety_level;   // 0-100
    uint16_t crc16;
} paw_wellness_payload_t;  // 10 bytes
```

### PAW_MSG_ENRICHMENT (0x40) — Hub → Feeder/Camera

Trigger enrichment on anxiety episode.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;            // 0x40
    uint8_t  node_id;         // 0x00
    uint8_t  seq;
    uint8_t  enrichment_type; // 1=treat, 2=audio, 3=voice
    uint8_t  target_node;     // feeder or camera
    uint8_t  intensity;       // 0-100
    uint16_t crc16;
} paw_enrichment_payload_t;  // 10 bytes
```

## Alert Flags

| Flag | Value | Meaning |
|------|-------|---------|
| PAW_ALERT_HRV_DECLINE | 0x01 | HRV >20% below baseline |
| PAW_ALERT_HR_ELEVATED | 0x02 | Resting HR >15% above baseline |
| PAW_ALERT_FEVER | 0x04 | Skin temp >0.5°C above baseline |
| PAW_ALERT_LAMENESS | 0x08 | Gait asymmetry > threshold |
| PAW_ALERT_SCRATCHING | 0x10 | Scratching >3× baseline |
| PAW_ALERT_APPETITE_LOSS | 0x20 | >25% food uneaten |
| PAW_ALERT_ANXIETY | 0x40 | Separation anxiety episode |
| PAW_ALERT_LOW_BATT | 0x80 | Battery <15% |

## Activity Classes

| Class | Name |
|-------|------|
| 0 | resting |
| 1 | walking |
| 2 | running |
| 3 | sleeping |
| 4 | scratching |
| 5 | head_shaking |
| 6 | licking |
| 7 | eating |
| 8 | playing |

## Behavior Classes

| Class | Name |
|-------|------|
| 0 | resting |
| 1 | pacing |
| 2 | vocalizing |
| 3 | destructive |
| 4 | elimination |
| 5 | playing |

## Vocalization Classes

| Class | Name |
|-------|------|
| 0 | none |
| 1 | pain |
| 2 | anxiety |
| 3 | alert |
| 4 | play |
| 5 | attention |
| 6 | distress |