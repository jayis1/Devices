# GlucoSync — Communication Protocol Specification

## Overview

GlucoSync uses a custom binary protocol over BLE 5.0 for inter-node communication and JSON/MQTT for cloud connectivity.

## Binary Protocol (BLE)

### Packet Format

```
┌─────────────────────────────────────────────────────────────────┐
│  0    1    2    3    4    5    6    7    8    9    10   11...   │
│ Sync0 Sync1 Ver MsgType SenderID  SeqNum Flags Len  Chk  Payload│
│  0x47  0x53       (1B)  (2B)      (2B)   (1B) (1B)(1B)  0-245B │
└─────────────────────────────────────────────────────────────────┘
```

- **Sync bytes**: `0x47 0x53` ('G' 'S') — packet start marker
- **Version**: `0x01` — protocol version
- **MsgType**: 1 byte — message type (see below)
- **SenderID**: 2 bytes — node identifier (little-endian)
- **SeqNum**: 2 bytes — sequence number (little-endian)
- **Flags**: 1 byte — bit 0: encrypted, bit 1: compressed, bit 2: ACK required
- **PayloadLen**: 1 byte — payload length (0-245)
- **Checksum**: 1 byte — XOR of bytes 0-9
- **Payload**: 0-245 bytes — message-specific data

### Node IDs

| Node | Base ID |
|------|---------|
| Hub | 0x0000 |
| Meal Scanner | 0x0100 |
| Activity Band | 0x0200 |
| Insulin Pen Tag | 0x0300 |

### Message Types

| Type | Code | Direction | Payload |
|------|------|-----------|---------|
| DATA_CGM | 0x01 | CGM → Hub | `payload_cgm_t` (12B) |
| DATA_MEAL | 0x02 | Scanner → Hub | `payload_meal_t` (12B) |
| DATA_ACTIVITY | 0x03 | Band → Hub | `payload_activity_t` (8B) |
| DATA_INSULIN | 0x04 | Pen Tag → Hub | `payload_insulin_t` (10B) |
| DATA_HUB_IMU | 0x05 | Internal | `payload_hub_imu_t` (16B) |
| CMD_MODE | 0x10 | Hub → Nodes | `payload_mode_t` (1B) |
| CMD_PAIR | 0x11 | Hub → Node | `payload_pair_t` (3B) |
| ALERT_HYPO | 0x20 | Hub → Nodes | `payload_alert_t` (5B) |
| ALERT_HYPER | 0x21 | Hub → Nodes | `payload_alert_t` (5B) |
| ALERT_CRITICAL | 0x22 | Hub → Nodes | `payload_alert_t` (5B) |
| FORECAST | 0x30 | Hub → Cloud | `payload_forecast_t` (14B) |
| ACK | 0x40 | Response | — |
| NACK | 0x41 | Response | — |
| HEARTBEAT | 0x50 | Hub → Nodes | — |
| STATUS | 0x51 | Node → Hub | `payload_status_t` (3B) |

### Payload Structures

#### payload_cgm_t (12 bytes)
```c
uint16_t glucose_mgdl;     // mg/dL, 0 = invalid
int16_t  trend_mgdl_min;  // rate of change, centi (150 = 1.50)
uint8_t  sensor_state;    // 0=ok, 1=warmup, 2=calibrating, 3=error
uint8_t  confidence;      // 0-100
uint32_t timestamp;        // Unix epoch seconds
```

#### payload_meal_t (12 bytes)
```c
uint16_t food_class_id;   // 0-199
uint8_t  food_confidence; // 0-100
uint16_t carb_grams;      // estimated carbs
uint16_t portion_grams;   // estimated portion
uint8_t  glycemic_index;  // 0-100
uint8_t  spectral_bands;  // bitmask of captured bands
uint32_t timestamp;
```

#### payload_activity_t (8 bytes)
```c
uint8_t  hr;              // bpm, 0=not computed
uint8_t  hrv_rmssd;       // RMSSD in ms
uint8_t  activity_class;  // 0=sedentary,1=walk,2=run,3=bike,4=strength
uint8_t  intensity;       // 0-100 (Karvonen)
uint8_t  confidence;
uint32_t timestamp;
```

#### payload_insulin_t (10 bytes)
```c
uint8_t  pen_type;       // 0=basal, 1=bolus
uint8_t  pen_id;         // 1-4
uint8_t  estimated_units;
uint8_t  confidence;
uint16_t injection_dur_ms;
uint32_t timestamp;
```

#### payload_forecast_t (14 bytes)
```c
uint16_t glucose_30min;  // predicted at t+30 (mg/dL)
uint16_t glucose_60min;  // predicted at t+60
uint8_t  hypo_risk_30;   // 0-100
uint8_t  hyper_risk_60;  // 0-100
uint8_t  risk_score;     // 0-100
uint8_t  recommendation; // 0=none,1=monitor,2=snack,3=insulin,4=check,5=help
uint32_t timestamp;
```

#### payload_alert_t (5 bytes)
```c
uint8_t  risk_score;
uint8_t  alert_level;     // 0=none,1=low,2=mod,3=high,4=critical
uint8_t  predicted_glucose; // predicted nadir
uint8_t  minutes_to_event;
uint8_t  duration_sec;
```

#### payload_mode_t (1 byte)
```c
uint8_t  mode;           // 0=active, 1=sleep, 2=fasting, 3=exercise
```

#### payload_status_t (3 bytes)
```c
uint8_t  battery_pct;    // 0-100
uint8_t  state;          // 0=idle, 1=active, 2=charging, 3=error
uint8_t  error_code;
```

## Cloud Protocol (MQTT)

### Topics

| Topic | Direction | Format | Frequency |
|-------|-----------|--------|-----------|
| `glucosync/data/forecast` | Hub → Cloud | JSON | Every 5 min |
| `glucosync/status/hub` | Hub → Cloud | JSON | Every 30 sec |
| `glucosync/alerts/critical` | Hub → Cloud | JSON | Event-driven |
| `glucosync/cmd/mode` | Cloud → Hub | Binary (1B) | On-demand |

### Forecast JSON

```json
{
  "glucose": 120,
  "trend": -0.5,
  "forecast_30": 95,
  "forecast_60": 78,
  "hypo_risk": 45,
  "risk": 35,
  "iob": 2.5,
  "cob": 15.0,
  "hr": 72,
  "activity": 0,
  "ts": 1783456789
}
```

## Security

- BLE payloads encrypted with AES-128-CTR (health data is sensitive)
- Per-session keys derived from ECDH P-256 key exchange during pairing
- Cloud communication over TLS 1.3
- All health data encrypted at rest in TimescaleDB