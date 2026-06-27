# CompostSync Protocol (CSP) Specification

## Overview

CSP is the communication protocol used between CompostSync nodes over LoRa 868 MHz. It uses TDMA (Time Division Multiple Access) for collision-free mesh communication and AES-128-CCM encryption for security.

## Physical Layer (LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868 MHz (EU ISM band) |
| Bandwidth | 125 kHz |
| Spreading Factor | SF11 |
| Coding Rate | 4/5 |
| TX Power | 17 dBm (50 mW, EU limit) |
| Preamble | 8 symbols |
| Sync Word | 0x34 (CompostSync) |
| Max Payload | 128 bytes (encrypted) |

## Packet Format

```
┌──────────┬──────────┬────────────────────┬────────────────────┬──────────┐
│ Preamble │ Sync     │ Header (11 bytes)  │ Payload (0-128 B) │ CRC16 (2B)│
│ 8 bytes  │ 4 bytes  │ type+ len+ seq+    │ AES-128-CCM        │ CCITT    │
│          │ 0x34     │ src+ dst+ timestamp│ encrypted          │          │
└──────────┴──────────┴────────────────────┴────────────────────┴──────────┘
```

### Header (11 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | msg_type | Message type (see below) |
| 1 | 1 | payload_len | Payload length (0-128) |
| 2 | 1 | seq_num | Sequence number (wraps) |
| 3 | 2 | src_id | Source node ID (little-endian) |
| 5 | 2 | dst_id | Destination node ID (0xFFFF = broadcast) |
| 7 | 4 | timestamp | Unix timestamp (set by radio layer) |

### Encryption

- Algorithm: AES-128-CCM (Counter with CBC-MAC)
- Key: 16 bytes (provisioned per network)
- Nonce: 12 bytes = SrcID(2) + SeqNum(1) + Padding(9)
- MAC: 4 bytes (truncated from 16-byte CCM tag)
- The payload + MAC are encrypted together

### CRC

- CRC-16 CCITT (polynomial 0x1021, init 0xFFFF)
- Computed over header + encrypted payload + MAC

## Message Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| DATA | 0x01 | Node→Hub | Sensor data report |
| CMD | 0x02 | Hub→Node | Command |
| ACK | 0x03 | Hub→Node | Acknowledgment |
| JOIN | 0x04 | Node→Hub | Network join request |
| SYNC | 0x05 | Hub→All | TDMA beacon/sync |
| ALERT | 0x06 | Node→Hub | High-priority alert |

## Node IDs

| ID | Node |
|-----|------|
| 0x0001 | Hub |
| 0x0002 | Bin Node #1 |
| 0x0003 | Bin Node #2 |
| 0x0004 | Weather Station |
| 0x0005 | Soil Probe |
| 0xFFFF | Broadcast |

## TDMA Schedule

5-slot frame, 1000 ms per slot, 5000 ms total:

```
Slot 0 (0-1000ms):    Hub → SYNC beacon (broadcast)
Slot 1 (1000-2000ms): Bin Node #1 → Hub (DATA)
Slot 2 (2000-3000ms): Bin Node #2 → Hub (DATA) [reserved]
Slot 3 (3000-4000ms): Weather Station → Hub (DATA)
Slot 4 (4000-5000ms): Hub → ACK/commands to nodes
```

Nodes listen for the SYNC beacon to synchronize their clocks. If a node misses 3 consecutive beacons, it enters a recovery mode (random backoff, re-join).

## Data Payloads

### Bin Node Data (25 bytes)

| Offset | Size | Field | Scaling |
|--------|------|-------|---------|
| 0 | 2 | node_id | — |
| 2 | 4 | uptime_s | seconds |
| 6 | 1 | battery_pct | 0-100% |
| 7 | 6 | temp_c[3] | int16 × 10 (552 = 55.2°C) |
| 13 | 6 | moisture_pct[3] | uint16 0-100 |
| 19 | 2 | co2_ppm | ppm |
| 21 | 2 | methane_ppm | ppm |
| 23 | 2 | mass_grams | grams |
| 25 | 1 | vent_position | 0-100% |
| 26 | 1 | phase | 0-5 |
| 27 | 1 | alerts | bitmask |

### Weather Station Data (20 bytes)

| Offset | Size | Field | Scaling |
|--------|------|-------|---------|
| 0 | 2 | node_id | — |
| 2 | 4 | uptime_s | seconds |
| 6 | 1 | battery_pct | 0-100% |
| 7 | 2 | temp_c | ×10 |
| 9 | 2 | humidity_pct | 0-100 |
| 11 | 2 | pressure_hpa | hPa |
| 13 | 2 | wind_speed_ms | ×10 |
| 15 | 2 | wind_dir_deg | 0-359 |
| 17 | 2 | rain_mm | ×10 |
| 19 | 1 | rssi_dbm | signed |

### Soil Probe Data (28 bytes)

| Offset | Size | Field | Scaling |
|--------|------|-------|---------|
| 0 | 2 | node_id | — |
| 2 | 4 | uptime_s | seconds |
| 6 | 1 | battery_pct | 0-100% |
| 7 | 8 | temp_c[4] | int16 ×10 |
| 15 | 6 | moisture_pct[3] | 0-100 |
| 21 | 2 | ph | int16 ×100 (650 = 6.50) |
| 23 | 2 | co2_ppm | ppm |
| 25 | 1 | alerts | bitmask |
| 26 | 2 | reserved | — |

## Alert Bitmask

| Bit | Flag | Meaning |
|-----|------|---------|
| 0 | METHANE_HIGH | Methane > 1000 ppm |
| 1 | OVERHEAT | Temp > 70°C |
| 2 | LOW_BATTERY | Battery < 20% |
| 3 | SENSOR_FAULT | Sensor reading failed |
| 4 | ANAEROBIC | Anaerobic conditions |
| 5 | MOISTURE_LOW | Moisture < 30% |
| 6 | MOISTURE_HIGH | Moisture > 70% |

## Commands

| Command | Value | Parameters |
|---------|-------|------------|
| OPEN_VENT | 0x10 | — |
| CLOSE_VENT | 0x11 | — |
| SET_VENT | 0x12 | param[0] = 0-100% |
| TARE_WEIGHT | 0x20 | — |
| SET_RATE | 0x30 | param[0] = seconds |
| REBOOT | 0xF0 | — |
| OTA_BEGIN | 0xF1 | — |
| OTA_CHUNK | 0xF2 | — |