# PoolSync Protocol (PSP) Specification

**Version:** 1.0  
**Date:** 2026-06-26

## Overview

PoolSync Protocol (PSP) is a binary, little-endian, CRC16-verified protocol for communication between pool monitoring nodes over Sub-GHz LoRa (868 MHz) and between the hub and cloud over Wi-Fi/MQTT.

## Physical Layer

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU ISM band) |
| Modulation | LoRa |
| Bandwidth | 125 kHz |
| Spreading Factor | SF9 |
| Coding Rate | 4/5 |
| TX Power | +14 dBm (25 mW) |
| Range | 2 km LOS, 500m urban |
| Preamble | 8 symbols |
| Sync Word | 0x12 (private network) |

## Frame Format

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬─────────┬──────┐
│ PREAMBLE │  SYNC    │  LEN     │  SRC     │  DST     │  TYPE    │ PAYLOAD │ CRC  │
│ 2 bytes  │ 2 bytes  │ 2 bytes  │ 2 bytes  │ 2 bytes  │ 1 byte   │ 0-200 B │2 B   │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴─────────┴──────┘
```

### Field Descriptions

| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| PREAMBLE | 0 | 2 | Fixed value `0x5AA5` — frame start marker |
| SYNC | 2 | 2 | Fixed value `0x5053` ("PS" — PoolSync) |
| LEN | 4 | 2 | Total frame length including header, payload, and CRC |
| SRC | 6 | 2 | Source node address (see address table) |
| DST | 8 | 2 | Destination node address (0xFFFF = broadcast) |
| TYPE | 10 | 1 | Message type identifier (see message types) |
| PAYLOAD | 11 | 0-200 | Message-specific payload |
| CRC | 11+len | 2 | CRC16-CCITT over entire frame (header + payload) |

### Address Table

| Address | Node |
|---------|------|
| 0x0001 | Hub |
| 0x0100–0x01FF | Chemistry Probe (probe_id = addr - 0x0100) |
| 0x0200 | Pool Camera |
| 0x0300 | Equipment Controller |
| 0x0400 | Solar Monitor |
| 0xFFFF | Broadcast |

## Message Types

| Type | ID | Direction | Description |
|------|----|-----------|-------------|
| CHEM_DATA | 0x01 | Probe→Hub | Chemistry sensor readings |
| IMAGE_DATA | 0x02 | Camera→Hub | Water clarity metadata |
| IMAGE_UPLOAD | 0x03 | Hub→Camera | Request full image over Wi-Fi |
| EQUIP_STATUS | 0x04 | Equip→Hub | Equipment status + sensor data |
| DOSE_COMMAND | 0x05 | Hub→Equip | Chemical dosing command |
| EQUIP_COMMAND | 0x06 | Hub→Equip | Equipment control command |
| SOLAR_DATA | 0x07 | Solar→Hub | Solar panel data |
| ALARM | 0x08 | Any→Hub | Safety alarm (contention slot) |
| HEARTBEAT | 0x10 | Any↔Hub | Keep-alive, battery, RSSI |
| PING | 0x11 | Any→Hub | Link quality check |
| PONG | 0x12 | Hub→Any | Link quality response |
| TIME_SYNC | 0x13 | Hub→Any | UTC timestamp synchronization |
| OTA_START | 0x20 | Hub→Any | Begin OTA update (total_size, CRC32) |
| OTA_CHUNK | 0x21 | Hub→Any | OTA firmware chunk |
| OTA_DONE | 0x22 | Any→Hub | OTA verification result |
| HUB_CONFIG | 0x30 | Cloud→Hub | Configuration update |
| HUB_STATE | 0x31 | Hub→Cloud | Full state snapshot |

## Payload Formats

### CHEM_DATA (0x01) — 22 bytes

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 4 | float32 | pH (0.00–14.00) |
| 4 | 4 | float32 | ORP in mV (-2000 to +2000) |
| 8 | 4 | float32 | Free chlorine ppm (0.00–10.00) |
| 12 | 4 | float32 | Water temperature °C |
| 16 | 4 | float32 | Conductivity µS/cm |
| 20 | 4 | float32 | Turbidity NTU |
| 24 | 2 | uint16 | Battery voltage mV |
| 26 | 1 | int8 | Radio RSSI dBm |

### DOSE_COMMAND (0x05) — 10 bytes

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 1 | uint8 | Pump ID (0=acid, 1=chlorine, 2=clarifier) |
| 1 | 4 | float32 | Volume mL |
| 5 | 2 | uint16 | Maximum duration seconds |
| 7 | 4 | uint32 | Unique command ID |

### EQUIP_COMMAND (0x06) — 6 bytes

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 1 | uint8 | Device ID (0=pump, 1=heater, ...7=spare) |
| 1 | 1 | uint8 | Command (0=off, 1=on, 2=toggle, 3=set_speed) |
| 2 | 2 | uint16 | Parameter (speed %, setpoint °C) |
| 4 | 2 | uint16 | Auto-off timer seconds (0=permanent) |

### EQUIP_STATUS (0x04) — 13 bytes

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 1 | uint8 | Relay states bitmask |
| 1 | 4 | float32 | Flow rate L/min |
| 5 | 4 | float32 | Filter pressure kPa |
| 9 | 4 | float32 | AC current A |
| 13 | 1 | uint8 | Pump status (0=idle, 1=running, 2=dosing, 3=fault) |
| 14 | 2 | uint16 | Battery mV (0 if mains) |
| 16 | 1 | int8 | Radio RSSI dBm |

### ALARM (0x08) — 40 bytes

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 1 | uint8 | Alarm type (see alarm types) |
| 1 | 1 | uint8 | Severity (0=info, 1=warning, 2=critical, 3=emergency) |
| 2 | 4 | float32 | Alarm-specific value |
| 6 | 4 | uint32 | Unix timestamp |
| 10 | 30 | uint8[] | Alarm-specific data |

### Alarm Types

| Type | ID | Description |
|------|----|-------------|
| ENTRAPMENT | 0x01 | Suction entrapment detected |
| GFCI_FAULT | 0x02 | Ground fault detected |
| UNAUTH_ACCESS | 0x03 | Unsupervised pool access |
| CHEM_OUTSIDE | 0x04 | Chemistry dangerously out of range |
| FREEZE | 0x05 | Freeze protection activated |
| EQUIP_FAULT | 0x06 | Equipment malfunction |
| LOW_BATTERY | 0x07 | Probe battery low |
| DOSE_FAIL | 0x08 | Dosing verification failed |

## TDMA Timing

```
Time (ms):  0    100   200   300   400   500   600   700   800   900
            │     │     │     │     │     │     │     │     │     │
Slot:       0     1     2     3     4     5     6     7     8     9
            Hub   Hub  Prb1  Prb2  Prb3  Cam   Equip Solar Alarm Free
            TX    RX    TX    TX    TX    TX    TX    TX    (any)  ─
```

- Hub transmits config/commands in slot 0
- Hub listens in slots 1-7
- Slot 8 is contention-based for urgent alarms
- Slot 9 is unassigned (future use)
- Total frame duration: 1000 ms (1 second)

## Encryption

All payloads are encrypted with AES-128-GCM before transmission:
- **Key**: Per-node-pair key (provisioned during pairing)
- **Nonce**: 12 bytes (SRC_addr + DST_addr + frame_counter)
- **Tag**: 16 bytes appended after payload (before CRC)
- **Replay protection**: Monotonically increasing frame_counter per node pair

## OTA Firmware Updates

1. Hub sends `OTA_START` with total_size and expected CRC32
2. Hub sends `OTA_CHUNK` frames with 180-byte chunks (200 - 20 header)
3. Node reassembles and verifies CRC32
4. Node sends `OTA_DONE` with verification result
5. If verification passes, node reboots into new firmware

## Error Handling

- **CRC failure**: Frame silently dropped
- **Unknown message type**: Frame silently dropped
- **Wrong destination**: Frame silently dropped
- **Replay attack (nonce too low)**: Frame silently dropped
- **Alarm collision (slot 8)**: Random backoff 0-3 slot durations, retransmit