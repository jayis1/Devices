# FreshKeep — Mesh Protocol Specification

## Overview

FreshKeep uses a custom TDMA (Time Division Multiple Access) protocol over LoRa 868MHz Sub-GHz radio for reliable, low-latency communication between kitchen nodes. The protocol is designed for:

- **Deterministic latency**: Stove Guard data always arrives within 100ms
- **Reliability**: CRC16 error detection + ACK/retransmit
- **Low power**: Nodes sleep between TDMA slots (fridge node: 3mA average)
- **Fire safety priority**: Stove Guard always gets Slot 0

## Physical Layer

| Parameter | EU (868MHz) | US (915MHz) |
|-----------|-------------|-------------|
| Frequency | 868.0 MHz | 915.0 MHz |
| Modulation | LoRa | LoRa |
| Spreading Factor | SF7 (normal) | SF7 (normal) |
| | SF9 (alerts) | SF9 (alerts) |
| Bandwidth | 125 kHz | 125 kHz |
| TX Power | +14 dBm (hub: +20) | +20 dBm (hub: +27) |
| Range (indoor) | 30m | 30m |
| Range (long range) | 200m | 500m |
| Coding Rate | 4/5 | 4/5 |
| Preamble | 8 symbols | 8 symbols |
| Sync Word | 0xF04F | 0xF04F |

## TDMA Frame Structure

```
Time →  0ms    100ms   200ms   300ms   400ms   500ms
        ┌───────┬───────┬───────┬───────┬───────┐
        │STOVE  │FRIDGE │PANTRY │HUB CMD│ CTRL  │
        │GUARD  │       │       │       │ ACK  │
        │Slot 0 │Slot 1 │Slot 2 │Slot 3 │Slot 4│
        └───────┴───────┴───────┴───────┴───────┘
        
        500ms total frame, repeats continuously

Fire Alarm Override:
        ┌───────────────────────────────────────────────┐
        │ FIRE ALARM BROADCAST (STOVE GUARD takes all)  │
        │ Hub immediately relays to all nodes + cloud   │
        └───────────────────────────────────────────────┘
```

### Slot Assignments

| Slot | Node | Duration | Purpose |
|------|------|----------|---------|
| 0 | Stove Guard | 100ms | Thermal + gas data (PRIORITY) |
| 1 | Fridge Node | 100ms | Gas + temp + weight data |
| 2 | Pantry Node | 100ms | Weight + barcode data |
| 3 | Hub | 100ms | Sync + commands broadcast |
| 4 | Any | 100ms | ACK + retransmit + OTA |

### Slot Timing
- Hub is TDMA coordinator, broadcasts sync beacon in Slot 3
- All nodes synchronize to hub beacon on power-up
- Slot 0 starts 0ms after sync beacon
- Maximum clock drift tolerance: ±500µs (node re-syncs every frame)

## Packet Format

```
┌─────────┬──────────┬──────┬──────┬──────┬──────┬──────────┬───────┐
│PREAMBLE │ SYNC     │ LEN  │ SRC  │ DST  │ TYPE │ PAYLOAD  │ CRC16 │
│ 4 bytes │ 2 bytes  │ 1B   │ 1B   │ 1B   │ 1B   │ 0-50B    │ 2B    │
└─────────┴──────────┴──────┴──────┴──────┴──────┴──────────┴───────┘

Total: 12-62 bytes
Over-the-air time (SF7): ~20-100ms (well within 100ms slot)
```

### Fields

| Field | Size | Description |
|-------|------|-------------|
| PREAMBLE | 4 bytes | 0xAA 0xAA 0xAA 0xAA |
| SYNC | 2 bytes | 0xF0 0x4F |
| LEN | 1 byte | Length from LEN to end of PAYLOAD |
| SRC | 1 byte | Source node address |
| DST | 1 byte | Destination node address (0xFF = broadcast) |
| TYPE | 1 byte | Packet type identifier |
| PAYLOAD | 0-50 bytes | Type-specific data |
| CRC16 | 2 bytes | CCITT CRC16 over LEN+SRC+DST+TYPE+PAYLOAD |

### Node Addresses

| Address | Node |
|---------|------|
| 0x00 | Hub |
| 0x01 | Fridge Node |
| 0x02 | Pantry Node |
| 0x03 | Stove Guard |
| 0xFF | Broadcast |

### Packet Types

| Type | Code | Payload Size | Description |
|------|------|--------------|-------------|
| FRIDGE_DATA | 0x01 | 28 bytes | Fridge sensor readings |
| PANTRY_DATA | 0x02 | 30 bytes | Pantry sensor readings |
| STOVE_DATA | 0x03 | 24 bytes | Stove guard readings |
| FIRE_ALARM | 0x04 | 10 bytes | Critical fire alarm |
| COMMAND | 0x05 | 10 bytes | Command to node |
| ACK | 0x06 | 2 bytes | Acknowledgment |
| OTA_BLOCK | 0x07 | 50 bytes | OTA firmware chunk |
| INVENTORY_UPDATE | 0x08 | 22 bytes | Item added/removed |
| HEARTBEAT | 0x09 | 4 bytes | Keep-alive |
| SHOPPING_LIST | 0x0A | variable | Shopping list from hub |

## Data Structures

### FRIDGE_DATA (0x01) — 28 bytes

| Offset | Size | Field | Type | Range | Unit |
|--------|------|-------|------|-------|------|
| 0 | 2 | voc_index | uint16 | 0-500 | SGP40 VOC index |
| 2 | 2 | co2_ppm | uint16 | 400-10000 | SCD30 CO2 ppm |
| 4 | 2 | ethylene_raw | uint16 | 0-4095 | MQ-3 ADC raw |
| 6 | 2 | temp_c_x10 | int16 | -200-500 | Temperature ×10 °C |
| 8 | 2 | humidity_x10 | uint16 | 0-1000 | Humidity ×10 % |
| 10 | 16 | weight_mg[4] | uint32×4 | 0-5000000 | Milligrams per shelf |
| 26 | 1 | door_state | uint8 | 0-1 | 0=closed, 1=open |
| 27 | 1 | spoilage_score | uint8 | 0-100 | Computed spoilage index |
| 28+ | 1 | image_ready | uint8 | 0-1 | New image available flag |

### STOVE_DATA (0x03) — 24 bytes

| Offset | Size | Field | Type | Range | Unit |
|--------|------|-------|------|-------|------|
| 0 | 2 | max_temp_c | uint16 | 0-500 | Max thermal pixel °C |
| 2 | 2 | avg_temp_c | uint16 | 0-500 | Avg hot zone °C |
| 4 | 2 | lpg_ppm | uint16 | 0-9999 | LPG concentration ppm |
| 6 | 2 | co_ppm | uint16 | 0-9999 | CO concentration ppm |
| 8 | 2 | nh3_ppm | uint16 | 0-9999 | NH3 concentration ppm |
| 10 | 1 | smoke_level | uint8 | 0-255 | Smoke density |
| 11 | 1 | flame_detected | uint8 | 0-1 | IR flame detection |
| 12 | 1 | burner_state | uint8 | 0-3 | 0=off, 1=low, 2=med, 3=high |
| 13 | 1 | motion_detected | uint8 | 0-1 | Person nearby |
| 14 | 1 | gas_valve_state | uint8 | 0-1 | 0=closed, 1=open |
| 15 | 1 | fire_confidence | uint8 | 0-255 | ML fire confidence |
| 16 | 1 | alert_level | uint8 | 0-4 | Current alert level |
| 18 | 2 | thermal_checksum | uint16 | 0-65535 | CRC of thermal frame |

### FIRE_ALARM (0x04) — 10 bytes

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 2 | max_temp_c | uint16 | Peak temperature °C |
| 2 | 2 | lpg_ppm | uint16 | LPG reading |
| 4 | 1 | smoke_level | uint8 | Smoke density |
| 5 | 1 | flame_detected | uint8 | Flame IR detected |
| 6 | 1 | fire_confidence | uint8 | ML fire confidence |
| 7 | 1 | source_node | uint8 | Which node detected |
| 8 | 2 | timestamp_ms | uint16 | Hub-relative timestamp |

## Reliability

### ACK/NACK
- Every data packet expects an ACK in Slot 4
- If no ACK received, node retransmits in next frame's Slot 4
- Maximum 3 retransmissions before reporting link failure

### Fire Alarm Override
- FIRE_ALARM packet uses broadcast address (0xFF)
- All nodes immediately stop normal TDMA and relay the alarm
- Hub forwards alarm to cloud via WiFi
- Normal TDMA resumes after 5 seconds of no alarm packets

### OTA Updates
- Hub distributes firmware blocks in Slot 3 (command slot)
- Nodes store blocks in external flash
- Complete image verified by CRC32 before applying
- Rollback to previous image if new firmware fails CRC check within 30 seconds

## Encryption (Future)

Current protocol is unencrypted for simplicity. Future versions will implement:
- AES-128-CCM encryption of payload
- Per-node session keys derived from ECDH key exchange
- Key rotation every 24 hours