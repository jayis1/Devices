# CalmGrid Mesh Protocol Specification

## Overview

CalmGrid uses a compact binary protocol over a BLE 5.3 mesh network. All payloads are CRC16-CCITT protected for integrity.

## Frame Format

```
┌──────────┬──────────┬──────────┬───────────────┬──────────┐
│ Type (1) │ Node (1) │ Seq (1)  │ Payload (N)   │ CRC16 (2)│
└──────────┴──────────┴──────────┴───────────────┴──────────┘
```

- **Type:** Message type (see `calm_msg_type_t`)
- **Node:** Source node ID (see `CALM_NODE_ID_*`)
- **Seq:** Sequence number (wraps at 255)
- **Payload:** Type-specific struct (packed, little-endian)
- **CRC16:** CRC16-CCITT over Type + Node + Seq + Payload

## Node IDs

| ID | Node |
|----|------|
| 0x00 | Hub |
| 0x01 | Wrist Band |
| 0x02 | Room Sentinel |
| 0x03 | Light Node |

## Message Types

### CALM_MSG_VITALS (0x10) — Wrist Band → Hub

Sent every 60 seconds. Contains HR, HRV, EDA, temperature, activity, and battery.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x10
    uint8_t  node_id;          // 0x01
    uint8_t  seq;
    uint8_t  flags;            // alert bitmask
    uint8_t  hr_bpm;           // heart rate (bpm)
    uint16_t hrv_rmssd;        // RMSSD (centi-ms → /100 = ms)
    uint16_t eda_scl;          // skin conductance level (µS * 100)
    uint16_t eda_scr_rate;     // SCR events/min * 100
    int16_t  skin_temp_centic; // skin temp (centi-degC)
    uint8_t  activity_class;   // 0-7
    uint8_t  confidence;       // 0-100
    uint16_t step_count;       // steps since last report
    uint8_t  battery_pct;      // 0-100
    uint16_t crc16;
} calm_vitals_payload_t;  // 20 bytes
```

### CALM_MSG_PROSODY (0x11) — Sentinel → Hub/Cloud

Sent every 60 seconds. Contains prosody stress classification (NO audio).

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x11
    uint8_t  node_id;          // 0x02
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  prosody_class;    // 0=calm 1=neutral 2=elevated 3=high
    uint8_t  confidence;       // 0-100
    uint16_t speech_minutes;   // minutes of speech in interval
    int16_t  f0_deviation;     // F0 deviation from baseline (cents * 10)
    uint16_t crc16;
} calm_prosody_payload_t;  // 11 bytes
```

### CALM_MSG_ENVIRONMENT (0x12) — Sentinel → Hub/Cloud

Sent every 60 seconds. Ambient environment data.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;            // CALM_ALERT_ENV_STRESS if threshold breached
    uint16_t ambient_lux;      // lux * 10
    uint16_t cct_kelvin;       // correlated color temperature (K)
    int16_t  temp_centic;      // room temp centi-degC
    uint16_t humidity_centi;   // RH * 100 (%)
    uint16_t noise_db_tenth;   // noise dB * 10
    uint16_t crc16;
} calm_environment_payload_t;  // 14 bytes
```

### CALM_MSG_LIGHTING (0x13) — Hub → Light / Light → Hub

Scene command (hub → light) or feedback (light → hub).

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  scene;            // CALM_SCENE_*
    uint8_t  brightness;       // 0-100 %
    uint16_t warm_kelvin;      // target warm CCT (0 = default)
    uint16_t cool_kelvin;      // target cool CCT (0 = default)
    uint16_t ambient_lux;      // light node feedback only
    uint16_t crc16;
} calm_lighting_payload_t;  // 13 bytes
```

### CALM_MSG_INTERVENTION (0x14) — Hub → Mesh

Trigger an intervention (breathing, soundscape, lighting, notification).

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  intervention_id;  // 0=breathing 1=soundscape 2=lighting 3=combined
    uint8_t  param1;           // breathing pattern / soundscape id
    uint8_t  param2;           // secondary param
    uint16_t duration_s;       // intervention duration
    uint16_t crc16;
} calm_intervention_payload_t;  // 11 bytes
```

### CALM_MSG_STRESS_SCORE (0x30) — Hub → Mesh Broadcast

Broadcast every 15 min after stress inference. Light nodes use this autonomously.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  stress_score;     // 0-100
    uint8_t  burnout_risk;     // 0-100 (14-day forecast)
    uint8_t  recovery_score;   // 0-100
    uint16_t crc16;
} calm_stress_score_payload_t;  // 10 bytes
```

## Alert Flags

| Flag | Value | Condition |
|------|-------|-----------|
| HRV_DECLINE | 0x01 | HRV >20% below baseline |
| HR_ELEVATED | 0x02 | Resting HR >10% above baseline |
| EDA_AROUSAL | 0x04 | SCR rate >2× baseline |
| PROSODY_STRESS | 0x08 | Voice prosody = high-stress sustained |
| ACUTE_STRESS | 0x10 | EDA + HRV + HR concurrent >2 min |
| POOR_SLEEP | 0x20 | Sleep HRV suppressed |
| ENV_STRESS | 0x40 | Environmental stressor (noise/heat/flicker) |
| LOW_BATT | 0x80 | Battery <15% |

## Lighting Scenes

| Scene | Value | Description |
|-------|-------|-------------|
| OFF | 0x00 | LEDs off |
| CIRCADIAN | 0x01 | Follow time-of-day schedule |
| WORK | 0x02 | Cool focused work light |
| DESTRESS | 0x03 | Warm low-CCT calming |
| BREATHING | 0x04 | Gentle pulse synced to breathing pace |
| SUNSET | 0x05 | Warm dimming for evening |
| SUNRISE | 0x06 | Gradual warm wake-up |