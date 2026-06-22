# SkinSync Mesh Protocol Specification

## Overview

SkinSync uses a compact binary protocol over a Sub-GHz (868/915 MHz) mesh network via SX1262. All payloads are CRC16-CCITT protected for integrity. UV patches relay neighbor packets (store-and-forward mesh) to extend range.

## Frame Format

```
┌──────────┬──────────┬──────────┬───────────────┬──────────┐
│ Type (1) │ Node (1) │ Seq (1)  │ Payload (N)   │ CRC16 (2)│
└──────────┴──────────┴──────────┴───────────────┴──────────┘
```

- **Type:** Message type (see `ss_msg_type_t`)
- **Node:** Source node ID (see `SS_NODE_ID_*`)
- **Seq:** Sequence number (wraps at 255)
- **Payload:** Type-specific struct (packed, little-endian)
- **CRC16:** CRC16-CCITT over Type + Node + Seq + Payload

## Node IDs

| ID | Node |
|----|------|
| 0x00 | Mirror Hub |
| 0x01-0x0F | UV Patches (up to 15) |
| 0x40 | Skin Scanner |
| 0x50-0x57 | Smart Dispensers (up to 8) |

## Message Types

### SS_MSG_TELEMETRY (0x10) — UV Patch → Hub

Sent every 1 min (active/outdoor) or 5 min (sleep/indoor). Contains UVA/UVB dose, skin temp, UV index, MED fraction, battery.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x10
    uint8_t  node_id;          // patch ID
    uint8_t  seq;
    uint8_t  flags;            // alert bitmask
    uint16_t uva_dose_delta;   // UVA dose since last report (J/m² * 10)
    uint16_t uvb_dose_delta;   // UVB dose since last report (J/m² * 10)
    uint16_t uva_total;        // cumulative UVA dose today (J/m² * 10)
    uint16_t uvb_total;        // cumulative UVB dose today (J/m² * 10)
    int16_t  skin_temp_centic; // skin temperature (centi-degC)
    uint8_t  uv_index;         // current UV index * 10 (0-30.0)
    uint8_t  med_fraction;     // MED fraction used today (0-100)
    uint8_t  battery_pct;      // 0-100
    uint16_t crc16;
} ss_telemetry_payload_t;  // 18 bytes
```

### SS_MSG_DISPENSE_CMD (0x20) — Hub → Dispenser

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x20
    uint8_t  node_id;          // 0x00 (hub)
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  slot;             // product slot (0-3)
    uint16_t amount_mg;        // amount to dispense in mg
    uint8_t  product_id;       // RFID product ID
    uint16_t crc16;
} ss_dispense_cmd_payload_t;  // 9 bytes
```

### SS_MSG_DISPENSE_ACK (0x21) — Dispenser → Hub

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x21
    uint8_t  node_id;          // 0x50 | slot
    uint8_t  seq;
    uint8_t  flags;            // SS_ALERT_LOW_PRODUCT if < 15%
    uint8_t  status;           // SS_DISPENSE_* (0=ok 1=empty 2=partial 3=timeout)
    uint8_t  slot;             // which slot
    uint16_t mg_dispensed;     // actual amount dispensed (mg)
    uint16_t mg_remaining;     // product remaining (mg)
    uint16_t crc16;
} ss_dispense_ack_payload_t;  // 11 bytes
```

### SS_MSG_SCAN_RESULT (0x30) — Scanner → Hub/Cloud

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x30
    uint8_t  node_id;          // 0x40
    uint8_t  seq;
    uint8_t  flags;            // SS_ALERT_LESION / SS_ALERT_CONDITION
    uint8_t  body_location;    // 0=face 1=left-arm ...
    uint8_t  condition_class;  // SS_COND_* (0-25)
    uint8_t  condition_conf;   // 0-100
    uint8_t  abcde_score;      // 0-100 (lesion risk; 0 = no lesion)
    uint8_t  skin_age;         // estimated skin age (years)
    uint16_t lesion_id;        // tracked lesion ID (0 = untracked)
    uint16_t crc16;
} ss_scan_result_payload_t;  // 14 bytes
```

### SS_MSG_RISK_SCORE (0x50) — Hub → Mesh Broadcast

Broadcast every 5 min after risk inference. Patch uses it for haptic alert thresholds.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;             // 0x50
    uint8_t  node_id;          // 0x00 (hub)
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  patch_id;        // which patch
    uint8_t  uv_status;       // SS_UV_* (0-4)
    uint8_t  med_fraction;    // 0-100
    uint8_t  skin_cancer_risk;// 0-100 (annual cumulative)
    uint8_t  skin_status;     // SS_SKIN_* (0-3)
    uint16_t hours_to_burn;   // hours until burn (0xFFFF = safe)
    uint16_t crc16;
} ss_risk_score_payload_t;  // 14 bytes
```

## Alert Flags

| Flag | Value | Condition |
|------|-------|-----------|
| MED_50 | 0x01 | UV dose reached 50% of personal MED |
| MED_70 | 0x02 | UV dose reached 70% of personal MED — seek shade |
| MED_90 | 0x04 | UV dose reached 90% of personal MED — burning imminent |
| FLUSH | 0x08 | Skin temp rise >2°C (possible burn onset) |
| LOW_BATT | 0x10 | Patch battery < 15% |
| LOW_PRODUCT | 0x20 | Dispenser cartridge < 15% remaining |
| LESION | 0x40 | Scanner detected lesion change (ABCDE > 50) |
| CONDITION | 0x80 | Scanner detected skin condition requiring attention |

## UV Status Codes

| Status | Value | Description |
|--------|-------|-------------|
| SAFE | 0 | MED < 50% |
| CAUTION | 1 | MED 50-70% |
| WARNING | 2 | MED 70-90% |
| DANGER | 3 | MED > 90% — burn imminent |
| BURNED | 4 | MED exceeded — damage occurred |

## Skin Condition Codes

| Code | Condition |
|------|-----------|
| 0 | Normal |
| 1-3 | Acne (comedonal, inflammatory, cystic) |
| 4-6 | Hyperpigmentation (melasma, PIH, solar lentigines) |
| 7-8 | Rosacea (erythematous, papulopustular) |
| 9-10 | Eczema, seborrheic dermatitis |
| 11-14 | Pre-cancer/cancer signs (AK, BCC, SCC, melanoma) |
| 15-25 | Other (vitiligo, fungal acne, dermatitis, etc.) |

## Fitzpatrick Skin Types & MED

| Type | Description | MED (J/m²) | Burn time at UV index 10 |
|------|-------------|-----------|--------------------------|
| I | Very fair, always burns | 200 | ~10 min |
| II | Fair, usually burns | 250 | ~15 min |
| III | Medium, sometimes burns | 350 | ~25 min |
| IV | Olive, rarely burns | 500 | ~40 min |
| V | Brown, very rarely burns | 800 | ~60 min |
| VI | Dark, never burns | 1200 | ~90 min |

## Erythema Effectiveness (ISO 17166)

The erythema action spectrum peaks at 298nm (UVB) and drops sharply in UVA:
- UVB (280-320nm): ~1.0 effectiveness (primary burn risk)
- UVA (320-400nm): ~0.05 effectiveness (aging, contributes to burn at high dose)

The UV patch computes MED fraction as:
```
effective_dose = (UVB_wm2 * 1.0 + UVA_wm2 * 0.05) * time_seconds
MED_fraction = effective_dose / personal_MED * 100
```

## Body Location Codes

| Code | Location |
|------|----------|
| 0 | Face |
| 1 | Left arm |
| 2 | Right arm |
| 3 | Chest |
| 4 | Back |
| 5 | Left leg |
| 6 | Right leg |
| 7 | Neck |
| 8 | Scalp |
| 9 | Hand |
| 10 | Foot |
| 11 | Abdomen |