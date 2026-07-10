# AllergySync — Protocol Specification

## Packet Format

All mesh packets share a common header + payload + CRC16:

```
┌──────────────────────────────────────────────────┐
│ Header (10 bytes)                                │
├────────┬────────┬──────┬──────┬──────┬──────┬─────┤
│ Version│ MsgType│ SrcID│ DstID│ Hops │ Flags│ Seq │
│ 1 byte │ 1 byte │1 byte│1 byte│1 byte│1 byte│2B  │
├────────┴────────┴──────┴──────┴──────┴──────┴─────┤
│ PayloadLen (2 bytes)                             │
├──────────────────────────────────────────────────┤
│ Payload (0-128 bytes)                            │
├──────────────────────────────────────────────────┤
│ CRC16 (2 bytes, CCITT-FALSE)                     │
└──────────────────────────────────────────────────┘
```

### Header Fields

| Field | Size | Description |
|-------|------|-------------|
| Version | 1 | Protocol version (0x01) |
| MsgType | 1 | Message type (see below) |
| SrcID | 1 | Source node ID (0 = hub, 0xFF = unassigned) |
| DstID | 1 | Destination node ID (0xFF = broadcast) |
| HopCount | 1 | incremented per mesh hop |
| Flags | 1 | bit0: encrypted, bit1: mesh-forwarded |
| Seq | 2 | Sequence number (per-source) |
| PayloadLen | 2 | Payload length in bytes (0-128) |

### CRC16

CRC-16/CCITT-FALSE: polynomial 0x1021, init 0xFFFF, no XOR-out.
Calculated over header + payload. Appended as 2 bytes big-endian.

## Message Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| BEACON | 0x01 | Hub → All | TDMA schedule + time sync |
| JOIN_REQ | 0x02 | Node → Hub | ECDH pubkey, request slot |
| JOIN_RSP | 0x03 | Hub → Node | Slot assignment, session key |
| TELEMETRY | 0x04 | Node → Hub | Sensor data |
| COMMAND | 0x05 | Hub → Node | Actuator command |
| ACK | 0x06 | Node → Hub | Command acknowledgment |
| HEARTBEAT | 0x07 | Node → Hub | Keep-alive |
| OTA_NOTIFY | 0x08 | Hub → Node | OTA available notification |
| OTA_CHUNK | 0x09 | Hub → Node | OTA binary chunk |
| MESH_FWD | 0x0A | Any → Any | Mesh forwarding wrapper |

## Telemetry Payloads

### Sentinel Telemetry (40 bytes)

| Offset | Size | Field | Scale |
|--------|------|-------|-------|
| 0 | 2 | PM1.0 | µg/m³ × 10 |
| 2 | 2 | PM2.5 | µg/m³ × 10 |
| 4 | 2 | PM10 | µg/m³ × 10 |
| 6 | 2 | CO₂ | ppm |
| 8 | 2 | VOC index | 0-500 |
| 10 | 2 | Temperature | °C × 100 (signed) |
| 12 | 2 | Humidity | % × 10 |
| 14 | 2 | Pressure | hPa |
| 16 | 1 | Pollen class | enum 0-6 |
| 17 | 1 | Pollen confidence | 0-100% |
| 18 | 2 | Pollen count | particles/L |
| 20 | 2 | Fan RPM | rpm |
| 22 | 1 | Battery % | 0-100 |
| 23 | 1 | Flags | bit0: sensor_error |
| 24 | 16 | Reserved | — |

### Window Node Telemetry (16 bytes)

| Offset | Size | Field | Scale |
|--------|------|-------|-------|
| 0 | 1 | Window state | 0=closed, 1=open, 2=partial |
| 1 | 1 | Position % | 0-100 |
| 2 | 2 | Light lux | lux |
| 4 | 2 | Battery voltage | mV |
| 6 | 1 | Battery % | 0-100 |
| 7 | 1 | Relay state | 0=off, 1=on |
| 8 | 1 | Motor fault | 0=ok, 1=stall, 2=overcurrent |
| 9 | 6 | Reserved | — |

### Wearable Tag Telemetry (24 bytes)

| Offset | Size | Field | Scale |
|--------|------|-------|-------|
| 0 | 2 | PM2.5 | µg/m³ × 10 |
| 2 | 2 | PM10 | µg/m³ × 10 |
| 4 | 1 | Pollen class | enum 0-6 |
| 5 | 1 | Activity class | enum 0-5 |
| 6 | 2 | Battery voltage | mV |
| 8 | 1 | Battery % | 0-100 |
| 9 | 2 | Exposure index | cumulative |
| 11 | 12 | Reserved | — |

## Command Payload (8 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Command type | See command enum |
| 1 | 1 | Parameter | e.g., position %, speed |
| 2 | 6 | Reserved | — |

## Commands

| Command | Value | Description |
|---------|-------|-------------|
| CLOSE_WINDOW | 0x01 | Close window fully |
| OPEN_WINDOW | 0x02 | Open window fully |
| SET_POSITION | 0x03 | Set position to param % |
| PURIFIER_ON | 0x04 | Turn on air purifier relay |
| PURIFIER_OFF | 0x05 | Turn off air purifier relay |
| RECALIBRATE | 0x06 | Re-run window calibration |
| OTA_BEGIN | 0x07 | Start OTA update |
| OTA_COMMIT | 0x08 | Commit OTA update |
| REBOOT | 0x09 | Reboot node |

## TDMA Frame

```
Time →
┌────────┬──────┬──────┬──────┬──────┬───┬──────┬──────┐
│ Slot 0 │ Slot1│ Slot2│ Slot3│ Slot4│...│Slot11│ Idle  │
│Beacon  │Node1 │Node2 │Node3 │Node4 │   │Node11│gap    │
│ 500ms  │500ms │500ms │500ms │500ms │   │500ms │       │
└────────┴──────┴──────┴──────┴──────┴───┴──────┴──────┘
|←──────────── Total frame: 6000 ms ──────────────→|
```

- Hub transmits beacon at slot 0 (every 6 s)
- Nodes transmit only in their assigned slot
- Contention: new nodes send JOIN_REQ during idle gap at end of frame
- Mesh forwarding: intermediate nodes re-broadcast in their own slot

## Encryption

- **Algorithm:** AES-128-CCM (counter + CBC-MAC)
- **Key exchange:** ECDH P-256 during join handshake
- **Session key derivation:** HKDF-SHA256(shared_secret, "AllergySync-v1")
- **Nonce:** 13 bytes = [slot(1) | frame_counter(4) | src_id(1) | padding(7)]
- **MIC:** 4 bytes (truncated from 16-byte CCM tag)
- **AAD:** Full packet header (10 bytes)