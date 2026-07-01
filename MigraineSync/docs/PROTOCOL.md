# MigraineSync — Communication Protocol Specification

## Overview

MigraineSync nodes communicate using a binary TLV (Type-Length-Value) protocol over two transports:
- **Sub-GHz 868 MHz** (Env Sentinel ↔ Hub) — TDMA mesh, AES-128-CCM encrypted
- **BLE 5.0 GATT** (Aura Band / Hydrate Tag ↔ Hub) — LE Secure Connections

## Frame Format

All frames (both Sub-GHz and BLE) use the same structure:

```
┌──────────┬──────────┬──────────┬─────────────────────┬──────────┐
│ SOP (1B) │ SEQ (1B) │ LEN (1B) │ PAYLOAD (0-128B)    │ CRC (2B) │
└──────────┴──────────┴──────────┴─────────────────────┴──────────┘
```

| Field | Size | Description |
|-------|------|-------------|
| SOP | 1 byte | Start of packet: `0xA5` |
| SEQ | 1 byte | Sequence number (wraps 0-255) |
| LEN | 1 byte | Payload length (0-128 bytes) |
| PAYLOAD | 0-128 bytes | TLV-encoded message body |
| CRC | 2 bytes | CRC-16/CCITT over SEQ + LEN + PAYLOAD |

## Payload TLV Encoding

The payload consists of one or more TLV fields:

```
┌──────────┬──────────┬─────────────────┐
│ TYPE (1B)│ LEN (1B) │ VALUE (LEN B)   │
└──────────┴──────────┴─────────────────┘
```

## Message Types

| Msg Type | Code | Source | Description |
|----------|------|--------|-------------|
| ENVIRONMENT | 0x01 | Env Sentinel | Full environment snapshot |
| VITALS | 0x02 | Aura Band | PPG-derived vitals |
| BAROMETRIC | 0x03 | Aura Band / Env Sentinel | Barometric pressure reading |
| LIGHT_DOSE | 0x04 | Aura Band / Env Sentinel | Ambient light measurement |
| HYDRATION | 0x05 | Hydrate Tag | Water intake measurement |
| ALERT | 0x06 | Hub | Alert notification |
| MANUAL_EVENT | 0x07 | Hub (relayed from app) | User-logged event |
| BATTERY | 0x08 | All nodes | Battery status |
| TIME_SYNC | 0x09 | Hub → nodes | Time synchronization beacon |
| PAIR_REQ | 0x0A | Node → Hub | Pairing request |
| PAIR_ACK | 0x0B | Hub → Node | Pairing acknowledgment |
| FIRMWARE_OTA | 0x0C | Hub → Node | OTA firmware update chunk |

## Payload Definitions

### ENVIRONMENT (0x01)
```
VALUE:
  pressure_hPa      : f32  (4 bytes)   — 300.0 to 1100.0
  pressure_delta_3h : f32  (4 bytes)   — -50.0 to +50.0 hPa
  light_lux         : f32  (4 bytes)   — 0.0 to 120000.0
  temp_c            : f32  (4 bytes)   — -40.0 to +85.0
  humidity_pct      : f32  (4 bytes)   — 0.0 to 100.0
  voc_index         : u16  (2 bytes)   — 0 to 500
  co2_ppm           : u16  (2 bytes)   — 400 to 5000
  noise_db          : u8   (1 byte)    — 30 to 120
Total: 25 bytes
```

### VITALS (0x02)
```
VALUE:
  hr_bpm            : u8   (1 byte)    — 30 to 220
  hrv_rmssd_ms      : f32  (4 bytes)   — 0.0 to 200.0
  spo2_pct          : u8   (1 byte)    — 70 to 100
  skin_temp_c       : f32  (4 bytes)   — 20.0 to 42.0
  activity_level    : u8   (1 byte)    — 0=sleep, 1=sedentary, 2=light, 3=moderate, 4=vigorous
Total: 11 bytes
```

### BAROMETRIC (0x03)
```
VALUE:
  pressure_hPa      : f32  (4 bytes)
  pressure_delta_3h : f32  (4 bytes)
  temp_c            : f32  (4 bytes)
Total: 12 bytes
```

### LIGHT_DOSE (0x04)
```
VALUE:
  lux               : f32  (4 bytes)
  cumulative_lux_min: f32  (4 bytes)   — running integral of lux × minutes
Total: 8 bytes
```

### HYDRATION (0x05)
```
VALUE:
  volume_ml         : f32  (4 bytes)   — cumulative intake since midnight
  sip_count         : u8   (1 byte)    — sips since last report
  bottle_weight_g   : f32  (4 bytes)   — current bottle weight
Total: 9 bytes
```

### ALERT (0x06)
```
VALUE:
  level             : u8   (1 byte)    — 0=info, 1=low, 2=moderate, 3=high
  message_len       : u8   (1 byte)
  message           : char[message_len]
Total: 2 + message_len bytes
```

### MANUAL_EVENT (0x07)
```
VALUE:
  event_type        : u8   (1 byte)    — 0=migraine_onset, 1=medication, 2=symptom, 3=sleep, 4=meal
  timestamp_unix    : u32  (4 bytes)   — Unix epoch seconds
  note_len          : u8   (1 byte)    — optional note
  note              : char[note_len]
Total: 6 + note_len bytes
```

### BATTERY (0x08)
```
VALUE:
  battery_pct       : u8   (1 byte)    — 0 to 100
  voltage_mv        : u16  (2 bytes)   — millivolts
Total: 3 bytes
```

### TIME_SYNC (0x09)
```
VALUE:
  unix_timestamp    : u32  (4 bytes)   — Unix epoch seconds
  tz_offset_min     : i16  (2 bytes)   — timezone offset in minutes
Total: 6 bytes
```

## Multi-TLV Messages

A single frame payload can contain multiple TLV fields. For example, the Aura Band sends VITALS + BAROMETRIC + LIGHT_DOSE + BATTERY in one frame:

```
PAYLOAD = [TLV: VITALS(0x02, 11B, ...)] [TLV: BAROMETRIC(0x03, 12B, ...)] [TLV: LIGHT_DOSE(0x04, 8B, ...)] [TLV: BATTERY(0x08, 3B, ...)]
Total payload: 4 + 11 + 4 + 12 + 4 + 8 + 4 + 3 = 50 bytes (fits in 128-byte max)
```

## Sub-GHz TDMA Mesh

- **Coordinator**: Hub (assigns time slots)
- **Nodes**: Env Sentinel (up to 4 per hub for multi-room)
- **Slot duration**: 100 ms per node, 1 s superframe (10 slots: 1 beacon + 8 data + 1 retransmit)
- **Frequency**: 868.1 MHz (EU) / 915.0 MHz (US), 125 kHz bandwidth
- **Modulation**: LoRa CSS, SF7, BW=125 kHz (mesh mode) or FSK (high-speed mode)
- **Encryption**: AES-128-CCM (16-byte key provisioned at pairing)
- **Retransmission**: 2 retries with exponential backoff for unacknowledged frames

## BLE GATT

- **Service UUID**: `6e400001-b5a3-f393-e0a9-e50e24dcca9e` (Nordic UART-like custom)
- **TX Characteristic** (node → hub): `6e400002-...` — notify, 20-byte MTU chunks
- **RX Characteristic** (hub → node): `6e400003-...` — write, 20-byte MTU chunks
- **Connection interval**: 15 ms (low latency for alerts) to 100 ms (power save)
- **MTU negotiation**: request 247 bytes on connection (nRF52840 supports up to 251)