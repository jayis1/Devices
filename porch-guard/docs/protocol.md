# PorchGuard — Mesh Protocol Specification

## Overview

PorchGuard uses a custom TDMA (Time Division Multiple Access) protocol over
LoRa 915MHz (US) / 868MHz (EU) Sub-GHz radio for reliable, low-latency
communication between porch nodes. The protocol is designed for:

- **Pirate/tamper priority:** Alerts override normal TDMA on SF12 (max range)
- **Deterministic latency:** Each node has a guaranteed time slot
- **Long-range mailbox:** Mailbox uses SF9/SF12 for curb-to-house reach
- **Low power:** Mailbox achieves <5µA in STOP, lock <3µA in SystemOFF
- **Reliability:** CRC16 error detection + ACK/retransmit

## Physical Layer

| Parameter | US (915MHz) | EU (868MHz) |
|-----------|-------------|-------------|
| Frequency | 915.0 MHz | 868.0 MHz |
| Modulation | LoRa | LoRa |
| Spreading Factor | SF7 (normal) | SF7 (normal) |
| | SF9 (mailbox) | SF9 (mailbox) |
| | SF12 (pirate/tamper) | SF12 (pirate/tamper) |
| Bandwidth | 125 kHz | 125 kHz |
| TX Power | +14 dBm (nodes) | +14 dBm (nodes) |
| | +20 dBm (hub) | +20 dBm (hub) |
| Range (indoor) | 30m | 30m |
| Range (SF9) | 200m | 200m |
| Range (SF12) | 500m+ | 500m+ |
| Coding Rate | 4/5 | 4/5 |
| Preamble | 8 symbols | 8 symbols |
| Sync Word | 0x5047 | 0x5047 |

## TDMA Frame Structure

```
Time →  0ms     100ms    200ms    300ms    400ms
        ┌───────┬───────┬───────┬───────┬───────┐
        │ HUB   │CAMERA │MAILBOX│ LOCK  │ CTRL  │
        │ CMD   │ DATA  │ DATA  │ DATA  │ ACK   │
        │Slot 0 │Slot 1 │Slot 2 │Slot 3 │Slot 4 │
        └───────┴───────┴───────┴───────┴───────┘

Total frame: 500ms
Slot 0: Hub broadcasts sync + commands + armed state
Slot 1: Porch camera uplink (events/alerts/telemetry)
Slot 2: Mailbox uplink (long-range SF9 when polled)
Slot 3: Lock uplink (usually BLE; Sub-GHz on fallback)
Slot 4: Control / ACK / retransmit / OTA
```

## Pirate/Tamper Alert Override

```
When porch camera detects pirate risk > 0.8:
  1. Camera immediately broadcasts PIRATE_ALERT on SF12 (max range)
  2. Hub halts normal TDMA
  3. Hub activates 100 dB siren
  4. Hub forwards clip ref to cloud + mobile app
  5. All nodes relay the alert
  6. Normal TDMA resumes after 5s of no alert packets

Same override applies to:
  - Lock forced-entry (tilt + motor back-EMF anomaly)
  - Mailbox tamper (smash-and-grab / fishing)
  - Camera cover/tilt detected
```

## Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2) ]

PREAMBLE: 0xAA 0xAA 0xAA 0xAA
SYNC:     0x5047 ("PG")
LEN:      payload length (0-50)
SRC_ID:   source node ID (0x00-0x03)
DST_ID:   destination (0xFF = broadcast)
TYPE:     packet type (see below)
PAYLOAD:  type-specific struct (0-50 bytes)
CRC16:    CRC-16/CCITT over LEN+SRC+DST+TYPE+PAYLOAD
```

## Packet Types

| Type | Code | Payload | Direction |
|------|------|---------|-----------|
| CAMERA_DATA | 0x01 | camera_data_payload_t (32B) | camera → hub |
| MAILBOX_DATA | 0x02 | mailbox_data_payload_t (24B) | mailbox → hub |
| LOCK_DATA | 0x03 | lock_data_payload_t (20B) | lock → hub |
| COMMAND | 0x04 | command_payload_t | hub → node |
| ACK | 0x05 | — | node → hub |
| OTA_BLOCK | 0x06 | firmware chunk | hub → node |
| CALIBRATION | 0x07 | sensor calibration | hub → node |
| PIRATE_ALERT | 0x08 | pirate_alert_payload_t (16B) | camera → broadcast |
| TAMPER_ALERT | 0x09 | tamper_alert_payload_t (14B) | node → broadcast |
| DELIVERY_EVENT | 0x0A | delivery_event_payload_t (18B) | node → hub |
| HEARTBEAT | 0x0B | armed state | hub → broadcast |
| CLIP_REF | 0x0C | clip_ref_payload_t (12B) | camera → hub |

## Node IDs

| ID | Node |
|----|------|
| 0x00 | Hub |
| 0x01 | Porch Camera |
| 0x02 | Mailbox |
| 0x03 | Lock |
| 0xFF | Broadcast |

See `firmware/common/mesh_protocol.h` for full struct definitions.