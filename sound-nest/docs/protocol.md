# SoundNest Communication Protocol Specification

## Overview

SoundNest uses a layered communication protocol stack for reliable data transfer between all nodes in the system.

## Protocol Stack

```
┌─────────────────────────────────────┐
│         Application Layer           │  Sound events, SPL, dose, masking
├─────────────────────────────────────┤
│         Security Layer              │  AES-128-CCM encryption
├─────────────────────────────────────┤
│         Mesh Network Layer          │  TDMA scheduling, ACK/retry
├─────────────────────────────────────┤
│         Radio Layer                 │  SX1262 LoRa @ 868MHz
└─────────────────────────────────────┘
```

## Packet Format

All Sub-GHz packets follow this format:

```
┌──────────┬──────────┬──────┬──────────┬──────────┬──────────┬─────────┬──────────┐
│ PREAMBLE │  SYNC    │ LEN  │   SRC    │   DST    │   TYPE   │   SEQ   │  PAYLOAD │   MIC   │
│ 4 bytes  │ 2 bytes  │1 byte│ 2 bytes  │ 2 bytes  │ 1 byte   │ 2 bytes │0-64 bytes│ 4 bytes │
│ 0xAAAA   │ 0x4E53   │      │          │          │          │         │          │ AES-CCM │
└──────────┴──────────┴──────┴──────────┴──────────┴──────────┴─────────┴──────────┴──────────┘

Total: 12 (header) + N (payload) + 4 (MIC) bytes
Maximum packet size: 12 + 64 + 4 = 80 bytes
```

## TDMA Schedule

The hub assigns time slots to each node in a 10-second superframe:

```
Time:  0s    1s    2s    3s    4s    5s    6s    7s    8s    9s   10s
       │ HUB │ S1  │ S2  │ S3  │ SP1 │ SP2 │ W1  │ W2  │ RX  │ GUARD │
       │     │     │     │     │     │     │     │     │     │       │
       │     │←─→ │←─→  │←─→  │←─→  │←─→  │←─→  │←─→  │     │       │
       │     │ 800ms per slot, 200ms guard between slots
```

- **HUB**: Hub broadcasts commands, time sync, OTA
- **S1-S3**: Room sensors send SPL + event reports
- **SP1-SP2**: Masking speakers send feedback
- **W1-W2**: Wearable tags send dose reports
- **RX**: Hub receive window for all nodes
- **GUARD**: 200ms guard time for clock drift

## Message Types

| Type | Code | Direction | Description |
|------|------|-----------|-------------|
| JOIN_REQ | 0x01 | Node → Hub | Request to join mesh |
| JOIN_ACK | 0x02 | Hub → Node | Join accepted |
| JOIN_NACK | 0x03 | Hub → Node | Join rejected |
| EVENT_REPORT | 0x04 | Sensor → Hub | Sound event detected |
| SPL_REPORT | 0x05 | Sensor → Hub | Periodic SPL reading |
| MASKING_CMD | 0x06 | Hub → Speaker | Masking command |
| ALERT_CMD | 0x07 | Hub → Wearable | Haptic/LED alert |
| CONFIG_UPDATE | 0x08 | Hub → Node | Configuration change |
| OTA_BLOCK | 0x09 | Hub → Node | Firmware update block |
| HEARTBEAT | 0x0B | Bidirectional | Keep-alive |
| DOSE_REPORT | 0x0C | Wearable → Hub | Sound dose update |
| MASKING_FEEDBACK | 0x0E | Speaker → Hub | Masking status |

## Encryption

All payload data is encrypted with AES-128-CCM:
- **Key**: Per-node session key derived from master network key
- **Nonce**: Source address + sequence number (13 bytes)
- **MIC**: 4-byte truncated CBC-MAC
- **Associated Data**: Message type byte

## Retransmission

- ACK required for EVENT_REPORT, MASKING_CMD, ALERT_CMD, CONFIG_UPDATE, OTA_BLOCK
- No ACK for SPL_REPORT, HEARTBEAT, DOSE_REPORT (best-effort)
- Max 3 retransmissions with exponential backoff
- 200ms ACK timeout per retransmission

## BLE Protocol

### Nordic UART Service (NUS)
- **Service UUID**: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
- **RX Characteristic**: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
- **TX Characteristic**: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
- Used for configuration, node management, firmware updates

### SoundNest Custom Service
- **Service UUID**: 7E210001-5EAB-4E8A-B534-3B2D6F8A7C9D
- **Sound Event Characteristic**: 7E210002 (Notify) — classification results
- **SPL Level Characteristic**: 7E210003 (Notify) — real-time SPL
- **Dose Characteristic**: 7E210004 (Notify) — daily dose %
- **Masking Control Characteristic**: 7E210005 (Write) — start/stop/adjust
- **Config Characteristic**: 7E210006 (Read/Write) — node configuration

## MQTT Topics

```
soundnest/{device_id}/events          — Sound event detections (QoS 1)
soundnest/{device_id}/spl             — SPL time-series (QoS 0)
soundnest/{device_id}/dose           — Sound dose updates (QoS 1)
soundnest/{device_id}/masking        — Masking status (QoS 0)
soundnest/{device_id}/config         — Configuration changes (QoS 1)
soundnest/{device_id}/ota            — Firmware updates (QoS 1)
soundnest/{device_id}/cmd            — Commands from cloud (QoS 1)
soundnest/{device_id}/nodes/{node_id} — Per-node data (QoS 0)
```