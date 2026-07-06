# SightSync — Communication Protocol Specification

## Overview

SightSync uses a custom binary protocol over BLE 5.0 and Sub-GHz 868 MHz for inter-node communication, and JSON/MQTT for cloud connectivity.

## Binary Protocol (BLE + Sub-GHz)

### Packet Format

```
┌─────────────────────────────────────────────────────────────────┐
│  0    1    2    3    4    5    6    7    8    9    10   11...   │
│ Sync0 Sync1 Ver MsgType SenderID  SeqNum Flags Len  Chk  Payload│
│  0x53  0x53       (1B)  (2B)      (2B)   (1B) (1B)(1B)  0-245B │
└─────────────────────────────────────────────────────────────────┘
```

- **Sync bytes**: `0x53 0x53` ('S' 'S') — packet start marker
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
| Desk Sentinel | 0x0100 |
| Eye Tag | 0x0200 |
| Lamp Node | 0x0300 |

### Message Types

| Type | Code | Direction | Payload |
|------|------|-----------|---------|
| DATA_BLINK | 0x01 | Eye Tag → Hub | `payload_blink_t` (8B) |
| DATA_DISTANCE | 0x02 | Desk → Hub | `payload_distance_t` (12B) |
| DATA_LIGHT | 0x03 | Desk → Hub | `payload_light_t` (12B) |
| DATA_POSTURE | 0x04 | Eye Tag → Hub | `payload_posture_t` (14B) |
| DATA_TEMP | 0x05 | Eye Tag → Hub | `payload_temp_t` (8B) |
| DATA_BLUE_DOSE | 0x06 | Tag/Desk → Hub | `payload_blue_dose_t` (8B) |
| CMD_LAMP | 0x10 | Hub → Lamp | `payload_lamp_cmd_t` (6B) |
| CMD_MODE | 0x11 | Hub → Nodes | `payload_mode_t` (1B) |
| CMD_PAIR | 0x12 | Hub → Node | `payload_pair_t` (3B) |
| ALERT_FATIGUE | 0x20 | Hub → App | `payload_fatigue_t` (8B) |
| ALERT_DISTANCE | 0x21 | Hub → App | `payload_dist_alert_t` (4B) |
| ALERT_DRY_EYE | 0x22 | Hub → App | `payload_dry_eye_t` (6B) |
| ALERT_BREAK | 0x23 | Hub → App | `payload_break_t` (3B) |
| FORECAST | 0x30 | Cloud → App | `payload_forecast_t` (16B) |
| ACK | 0x40 | Response | — |
| NACK | 0x41 | Response | — |
| HEARTBEAT | 0x50 | Hub → Nodes | — |
| STATUS | 0x51 | Node → Hub | `payload_status_t` (3B) |

### Payload Structures

#### payload_blink_t (8 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | blinks_per_min | uint8 | Computed blink rate |
| 1 | blink_confidence | uint8 | 0-100 |
| 2 | blink_rate_quality | uint8 | 0=poor,1=fair,2=good |
| 3 | blink_ir_amplitude | uint8 | IR reflectance amplitude |
| 4-7 | timestamp | uint32 | Unix epoch seconds |

#### payload_distance_t (12 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0-1 | distance_mm | uint16 | Eye-to-screen distance (0=out of range) |
| 2 | distance_quality | uint8 | 0=invalid,1=low,2=med,3=high |
| 3 | near_work_flag | uint8 | 1 if <300mm sustained >5min |
| 4-7 | near_work_minutes | uint32 | Cumulative near-work minutes today |
| 8-11 | timestamp | uint32 | Unix epoch seconds |

#### payload_light_t (12 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0-1 | ambient_lux | uint16 | VEML7700 illuminance |
| 2-3 | blue_light_mw | uint16 | Blue-light irradiance (mW/m² × 10) |
| 4-5 | cct_estimate | uint16 | Estimated CCT (K) |
| 6 | blue_dose_today | uint8 | Cumulative blue-light dose |
| 7 | blue_dose_pct | uint8 | % of daily safe limit |
| 8 | ambient_quality | uint8 | 0=insufficient,1=adequate,2=good |
| 9-11 | timestamp | uint32 | Unix epoch seconds |

#### payload_posture_t (14 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0-1 | pitch_centi | int16 | Pitch angle (centi-degrees) |
| 2-3 | roll_centi | int16 | Roll angle (centi-degrees) |
| 4-5 | yaw_centi | int16 | Yaw angle (centi-degrees) |
| 6 | forward_head_flag | uint8 | 1 if forward head >15° sustained |
| 7 | posture_risk | uint8 | 0-100 (from edge CNN) |
| 8-11 | timestamp | uint32 | Unix epoch seconds |

#### payload_temp_t (8 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0-1 | temp_centi | int16 | Temperature (centi-Celsius) |
| 2-3 | temp_delta_centi | int16 | Delta from baseline |
| 4-7 | timestamp | uint32 | Unix epoch seconds |

#### payload_lamp_cmd_t (6 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0-1 | target_cct | uint16 | Target CCT (1800-6500 K) |
| 2 | brightness_pct | uint8 | Target brightness (0-100) |
| 3 | mode | uint8 | 0=auto,1=manual,2=circadian,3=reading |
| 4 | transition_sec | uint8 | Transition duration |
| 5 | reserved | uint8 | Reserved |

#### payload_fatigue_t (8 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | fatigue_score | uint8 | 0-100 |
| 1 | alert_level | uint8 | 0=none,1=low,2=moderate,3=high,4=critical |
| 2 | blink_rate | uint8 | Current blink rate (bpm) |
| 3 | minutes_since_break | uint8 | Minutes since last 20-20-20 |
| 4-5 | viewing_distance_mm | uint16 | Current distance |
| 6-7 | ambient_lux | uint16 | Current ambient lux |

#### payload_forecast_t (16 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | risk_30day | uint8 | 30-day risk % |
| 1 | risk_90day | uint8 | 90-day risk % |
| 2-3 | refractive_delta | int16 | Projected change (centi-diopter) |
| 4-5 | near_work_today | uint16 | Near-work minutes today |
| 6-7 | outdoor_today | uint16 | Outdoor-light minutes today |
| 8-9 | avg_distance_mm | uint16 | Average viewing distance today |
| 10 | recommendation | uint8 | 0=none,1=more_outdoor,2=reduce_near,3=checkup |
| 11 | reserved | uint8 | Reserved |
| 12-15 | timestamp | uint32 | Unix epoch seconds |

## Sub-GHz TDMA Schedule

| Time | Node | Action |
|------|------|--------|
| T+0 ms | Hub | Broadcast HEARTBEAT beacon |
| T+100 ms | Desk Sentinel | TX slot (distance, light data) |
| T+200 ms | Lamp Node | TX slot (status, ambient lux) |
| T+300-400 ms | All | ACK window |
| T+1000 ms | Hub | Next beacon (1s period) |

## Encryption

All Sub-GHz and BLE packets with flag bit 0 set are encrypted with AES-128-CTR. The encryption key is provisioned during pairing and stored in NVS (hub) or flash (nodes).

## MQTT Topics (Cloud)

```
sightsync/{user_id}/hub/blink         — blink rate data
sightsync/{user_id}/hub/distance      — viewing distance
sightsync/{user_id}/hub/light         — ambient + blue light
sightsync/{user_id}/hub/posture       — head posture
sightsync/{user_id}/hub/fatigue       — visual fatigue index
sightsync/{user_id}/hub/forecast      — myopia forecast
sightsync/{user_id}/hub/status       — node heartbeats
sightsync/{user_id}/cloud/lamp_cmd    — lamp command (downlink)
sightsync/{user_id}/cloud/policy      — DQN policy update (downlink)
```