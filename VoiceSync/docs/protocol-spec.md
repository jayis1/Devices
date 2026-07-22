# VoiceSync — Communication Protocol Specification

## Physical Layer

### Sub-GHz (Room Sentinel, Humidity Node)
- **Band:** 868 MHz (EU) / 915 MHz (US) Sub-GHz ISM
- **Modulation:** LoRa (SX1262)
- **Spreading Factor:** SF7–SF11 (adaptive, default SF9)
- **Bandwidth:** 125 kHz
- **Coding Rate:** 4/5
- **TX Power:** +22 dBm
- **Preamble:** 8 symbols
- **Range:** 300 m LOS (SF7), up to 2 km (SF11 + mesh relay)

### BLE 5.0 (Vocal Band, Hydration Tag)
- **Band:** 2.4 GHz ISM
- **Modulation:** GFSK
- **TX Power:** +8 dBm
- **Range:** 10 m line-of-sight
- **Connection:** GATT notifications to Hub (central)

## Link Layer (TDMA Mesh — Sub-GHz)

- **Slot count:** 20 (16 nodes + 4 relay)
- **Slot duration:** 50 ms
- **Frame duration:** 1000 ms
- **Hub node ID:** 0x00
- **Encryption:** AES-128-CCM (shared network key + per-node session key)
- **Topology:** Star-of-stars with mesh relay for far nodes
- **Max nodes:** 16 (4 room + 1 humidity + 4 vocal bands + 4 hydration + 3 spare)

## Message Format

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x56 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

- **Sync:** `0x56 0x53` ("VS" = VoiceSync)
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
| 0x0B | VOICE_STATUS | Hub→All | Vocal health score + risk |
| 0x0C | TIME_SYNC | Hub→Node | Epoch time |
| 0x0D | CONFIG | Hub→Node | Sampling config |
| 0x0E | CONFIG_ACK | Node→Hub | Config confirmed |
| 0x0F | VOICE_ALERT | Sentinel→Hub | Voice quality alert |

## Telemetry Payloads

### Vocal Band (Sub-type 0x01, 21 bytes via BLE)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | F0 | 2 | ×0.1 Hz |
| 4 | Jitter | 2 | ×0.01% |
| 6 | Shimmer | 2 | ×0.01% |
| 8 | HNR | 1 | ×1 dB |
| 9 | Phonation % | 1 | ×1% |
| 10 | Intensity | 1 | ×1 dB SPL (offset 40) |
| 11 | Pitch range | 2 | ×0.1 semitones |
| 13 | Neck angle | 2 | ×0.1° (signed) |
| 15 | Skin temp | 2 | ×0.01°C (offset 20) |
| 17 | Heart rate | 1 | ×1 bpm |
| 18 | HRV | 1 | ×1 ms |
| 19 | Stress | 1 | 0–100 |
| 20 | RSSI | 1 | signed dBm |

### Room Sentinel (Sub-type 0x02, 16 bytes via Sub-GHz)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V (USB=0xFF) |
| 2 | Voice quality class | 1 | 0–7 |
| 3 | Confidence | 1 | ×1% |
| 4 | F0 | 2 | ×0.1 Hz |
| 6 | Phonation % | 1 | ×1% |
| 7 | Temperature | 2 | ×0.1°C |
| 9 | Humidity | 2 | ×0.1% RH |
| 11 | VOC index | 2 | 0–500 |
| 13 | dB SPL | 1 | ×1 dB |
| 14 | Talking | 1 | 0/1 |
| 15 | RSSI | 1 | signed dBm |

### Hydration Tag (Sub-type 0x03, 10 bytes via BLE)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Water mass | 2 | ×1 g |
| 4 | Sips 24h | 2 | count |
| 6 | Intake 24h | 2 | ×1 mL |
| 8 | Last sip ago | 1 | ×1 min |
| 9 | RSSI | 1 | signed dBm (0xFF=BLE) |

### Humidity Node (Sub-type 0x04, 10 bytes via Sub-GHz)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x04 |
| 1 | Battery | 1 | ×0.01 V (USB=0xFF) |
| 2 | Temperature | 2 | ×0.1°C |
| 4 | Humidity | 2 | ×0.1% RH |
| 6 | Tank level | 1 | ×1% |
| 7 | Humidifier on | 1 | 0/1 |
| 8 | Fan on | 1 | 0/1 |
| 9 | RSSI | 1 | signed dBm |

## Alert Types

| Type | Alert | Severity |
|------|-------|----------|
| 0x01 | Low battery | Warning |
| 0x02 | Vocal rest needed | Info |
| 0x03 | High voice disorder risk | Critical |
| 0x04 | Hoarseness detected | Warning |
| 0x05 | Reflux pattern detected | Warning |
| 0x06 | Low humidity | Warning |
| 0x07 | Dehydration | Warning |
| 0x08 | Vocal fold anomaly | Critical |
| 0x09 | Node offline | Warning |
| 0x0A | Sensor anomaly | Warning |
| 0x0B | Tank empty | Info |
| 0x0C | Poor posture sustained | Warning |

## Command Types

| Type | Command | Target |
|------|---------|--------|
| 0x01 | HUMIDIFIER_ON | Humidity Node |
| 0x02 | HUMIDIFIER_OFF | Humidity Node |
| 0x03 | FAN_ON | Humidity Node |
| 0x04 | FAN_OFF | Humidity Node |
| 0x05 | BUZZER_ON | Hub |
| 0x06 | BUZZER_OFF | Hub |
| 0x07 | HIGH_RISK_MODE | All nodes |
| 0x08 | NORMAL_MODE | All nodes |
| 0x09 | SET_CONFIG | Any node |
| 0x0A | REBOOT | Any node |
| 0x0B | CALIBRATE | Any node |
| 0x0C | START_RECORDING | Room Sentinel |
| 0x0D | STOP_RECORDING | Room Sentinel |

## Voice Status Broadcast

| Offset | Field | Size | Value |
|--------|-------|------|-------|
| 0 | Risk level | 1 | 0=low, 1=mod, 2=high, 3=critical |
| 1 | Vocal health score | 1 | 0–100 |
| 2 | Disorder risk | 1 | 0–100 |
| 3 | Phonation % (today) | 1 | 0–100 |
| 4 | Hydration % | 1 | 0–100 |
| 5 | Rest recommended | 1 | 0/1 |
| 6 | Rest min remaining | 2 | minutes |

## Voice Quality Classes (VoiceNet)

| Class | Name | Clinical Significance |
|-------|------|----------------------|
| 0 | Normal | Healthy vocal folds |
| 1 | Hoarse | Vocal fold edema, nodules |
| 2 | Breathy | Vocal fold closure insufficiency |
| 3 | Strained | Muscle tension dysphonia |
| 4 | Tremor | Essential voice tremor |
| 5 | Fatigue | Cumulative vocal fatigue |
| 6 | Reflux | Laryngopharyngeal reflux (LPR) |
| 7 | Disorder | Pathology requiring clinical eval |

## Join Process

1. New node powers on → sends `JOIN_REQ` to hub (dst=0x00, src=0xFF)
2. Hub assigns node ID + TDMA slot (or BLE handle) → sends `JOIN_ACK`
3. Node stores assignment → begins transmitting in assigned slot
4. Hub broadcasts `TIME_SYNC` every 60 seconds for slot alignment