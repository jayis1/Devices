# MosquitoSync — Communication Protocol Specification

## Physical Layer

- **Band:** 868 MHz (EU) / 915 MHz (US) Sub-GHz ISM
- **Modulation:** LoRa (SX1262)
- **Spreading Factor:** SF7–SF11 (adaptive, default SF9)
- **Bandwidth:** 125 kHz
- **Coding Rate:** 4/5
- **TX Power:** +22 dBm
- **Preamble:** 8 symbols
- **Range:** 300 m LOS (SF7), up to 2 km (SF11 + mesh relay)

## Link Layer (TDMA Mesh)

- **Slot count:** 30 (24 nodes + 6 relay slots)
- **Slot duration:** 50 ms
- **Frame duration:** 1500 ms
- **Hub node ID:** 0x00
- **Encryption:** AES-128-CCM (shared network key + per-node session key)
- **Topology:** Star-of-stars with mesh relay for far nodes
- **Max nodes:** 24 (8 acoustic + 3 traps + 12 barriers + 1 weather)

## Message Format

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x6D 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

- **Sync:** `0x6D 0x53` ("MS" = MosquitoSync)
- **Src:** Source node ID (0xFF = unassigned during join)
- **Dst:** Destination (0xFF = broadcast)
- **MsgType:** 1 byte message type
- **MsgId:** 2-byte sequence number (little-endian)
- **Payload:** Variable length (0–240 bytes)
- **CRC:** CRC-16-CCITT (poly 0x1021, init 0xFFFF) over all preceding bytes

## Message Types

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | JOIN_REQ | Node→Hub | Capabilities, battery, FW version |
| 0x02 | JOIN_ACK | Hub→Node | NodeID, slot assignment |
| 0x03 | TELEMETRY | Node→Hub | Sensor readings (type-specific) |
| 0x04 | COMMAND | Hub→Node | Actuator commands |
| 0x05 | COMMAND_ACK | Node→Hub | Command result |
| 0x06 | ALERT | Node→Hub | Threshold breach, fault |
| 0x07 | OTA_BLOCK | Hub→Node | Firmware chunk |
| 0x08 | OTA_ACK | Node→Hub | Chunk ack |
| 0x09 | HEARTBEAT | Node→Hub | Alive + battery + RSSI |
| 0x0A | MESH_RELAY | Node→Node | Forwarded message |
| 0x0B | RISK_STATUS | Hub→All | BiteRisk + DiseaseRisk scores |
| 0x0C | TIME_SYNC | Hub→Node | Epoch time |
| 0x0D | CONFIG | Hub→Node | Sampling config |
| 0x0E | CONFIG_ACK | Node→Hub | Config confirmed |
| 0x0F | SPECIES_ALERT | Sentinel→Hub | Species detected + confidence |

## Telemetry Payloads

### Acoustic Sentinel (Sub-type 0x01, 14 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Temperature | 1 | ×1 °C (signed) |
| 3 | Humidity | 1 | ×1 % |
| 4 | Mosquito detected | 1 | 0/1 |
| 5 | Species class | 1 | 0–7 |
| 6 | Confidence | 1 | ×1 % |
| 7 | Wingbeat freq | 2 | ×0.1 Hz |
| 9 | Detections (24h) | 2 | count |
| 11 | Audio energy | 2 | ×0.01 |
| 13 | RSSI | 1 | signed dBm |

### CO2 Trap (Sub-type 0x02, 20 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Temperature | 2 | ×0.1 °C |
| 4 | Humidity | 2 | ×0.1 % |
| 6 | Pressure | 2 | ×0.1 hPa (offset 800) |
| 8 | Rain tips | 2 | ×0.2 mm |
| 10 | IR beam breaks | 2 | count |
| 12 | Capture count (24h) | 2 | count |
| 14 | Trap fullness | 1 | ×1 % |
| 15 | CO2 on | 1 | 0/1 |
| 16 | Propane level | 1 | ×1 % |
| 17 | Fan speed | 1 | ×1 % |
| 18 | Dominant species | 1 | 0–7 |
| 19 | RSSI | 1 | signed dBm |

### Window Barrier (Sub-type 0x03, 8 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Screen status | 1 | 0=open, 1=closed, 2=moving |
| 3 | Last trigger | 1 | 0=manual, 1=hub, 2=auto |
| 4 | Cycles (24h) | 1 | count |
| 5 | Motor current | 2 | ×0.01 A |
| 7 | RSSI | 1 | signed dBm |

### Weather Sentinel (Sub-type 0x04, 15 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x04 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Temperature | 2 | ×0.1 °C |
| 4 | Humidity | 2 | ×0.1 % |
| 6 | Pressure | 2 | ×0.1 hPa (offset 800) |
| 8 | Wind speed | 2 | ×0.1 m/s |
| 10 | Wind direction | 2 | ×1 degree |
| 12 | Rain tips | 2 | ×0.2 mm |
| 14 | RSSI | 1 | signed dBm |

## Alert Types

| Type | Alert | Severity |
|------|-------|----------|
| 0x01 | Low battery | Warning |
| 0x02 | Mosquito detected | Info |
| 0x03 | Disease vector detected | Critical |
| 0x04 | Trap full | Warning |
| 0x05 | Propane low | Warning |
| 0x06 | Propane leak | Critical |
| 0x07 | Barrier stuck | Error |
| 0x08 | High risk | Warning |
| 0x09 | Disease outbreak | Critical |
| 0x0A | Node offline | Warning |
| 0x0B | Sensor anomaly | Warning |
| 0x0C | Heater fault | Critical |

## Command Types

| Type | Command | Target |
|------|---------|--------|
| 0x01 | BARRIER_CLOSE | Barrier nodes |
| 0x02 | BARRIER_OPEN | Barrier nodes |
| 0x03 | TRAP_CO2_ON | Trap nodes |
| 0x04 | TRAP_CO2_OFF | Trap nodes |
| 0x05 | TRAP_FAN_ON | Trap nodes |
| 0x06 | TRAP_FAN_OFF | Trap nodes |
| 0x07 | ALARM_ON | Hub |
| 0x08 | ALARM_OFF | Hub |
| 0x09 | HIGH_RISK_MODE | All nodes |
| 0x0A | NORMAL_MODE | All nodes |
| 0x0B | SET_CONFIG | Any node |
| 0x0C | REBOOT | Any node |
| 0x0D | CALIBRATE | Any node |
| 0x0E | CAPTURE_IMAGE | Trap nodes |

## Join Process

1. New node powers on → sends `JOIN_REQ` to hub (dst=0x00, src=0xFF)
2. Hub assigns node ID + TDMA slot → sends `JOIN_ACK`
3. Node stores assignment → begins transmitting in assigned slot
4. Hub broadcasts `TIME_SYNC` every 60 seconds for slot alignment

## Risk Status Broadcast

When BiteRisk or DiseaseRisk exceeds thresholds, hub broadcasts `RISK_STATUS`:

| Offset | Field | Size | Value |
|--------|-------|------|-------|
| 0 | Risk level | 1 | 0=low, 1=mod, 2=high, 3=critical |
| 1 | BiteRisk score | 1 | 0–100 |
| 2 | DiseaseRisk score | 1 | 0–100 |
| 3 | Activity index | 1 | 0–100 |
| 4 | Peak time (min) | 2 | minutes until predicted peak |