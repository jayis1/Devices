# CradleKeep — Mesh Protocol Specification

## Overview

CradleKeep uses a custom TDMA (Time Division Multiple Access) protocol over LoRa 868MHz Sub-GHz radio for reliable, low-latency communication between nursery nodes. The protocol is designed for:

- **Safety priority**: Crib Pad breathing data always arrives within 100ms
- **Deterministic latency**: Each node has a guaranteed time slot
- **Low power**: Crib Pad achieves <10µA average (18+ months on coin cell)
- **Reliability**: CRC16 error detection + ACK/retransmit

## Physical Layer

| Parameter | EU (868MHz) | US (915MHz) |
|-----------|-------------|-------------|
| Frequency | 868.0 MHz | 915.0 MHz |
| Modulation | LoRa | LoRa |
| Spreading Factor | SF7 (normal) | SF7 (normal) |
| | SF9 (alerts) | SF9 (alerts) |
| Bandwidth | 125 kHz | 125 kHz |
| TX Power | +14 dBm (nodes) | +20 dBm (nodes) |
| | +20 dBm (hub) | +27 dBm (hub) |
| Range (indoor) | 30m | 30m |
| Range (long range) | 200m | 500m |
| Coding Rate | 4/5 | 4/5 |
| Preamble | 8 symbols | 8 symbols |
| Sync Word | 0x0C4B | 0x0C4B |

## TDMA Frame Structure

```
Time →  0ms    100ms   200ms   300ms   400ms   500ms
        ┌───────┬───────┬───────┬───────┬───────┐
        │CRIB   │NURSERY│FEEDING│ HUB   │ CTRL  │
        │PAD    │MONITOR│STATION│ CMD   │ ACK  │
        │Slot 0 │Slot 1 │Slot 2 │Slot 3 │Slot 4│
        └───────┴───────┴───────┴───────┴───────┘
        
        500ms total frame, repeats continuously

Breathing Alert Override:
        ┌───────────────────────────────────────────────┐
        │ BREATHING ALERT BROADCAST (CRIB PAD takes all)│
        │ Hub immediately relays to app + cloud         │
        └───────────────────────────────────────────────┘
```

### Slot Assignments

| Slot | Node | Duration | Purpose |
|------|------|----------|---------|
| 0 | Crib Pad | 100ms | Breathing + movement + position (HIGHEST PRIORITY) |
| 1 | Nursery Monitor | 100ms | Cry + environment + camera data |
| 2 | Feeding Station | 100ms | Weight + temperature + feeding state |
| 3 | Hub | 100ms | Sync + commands broadcast |
| 4 | Any | 100ms | ACK + retransmit + OTA |

### Slot Timing

- Hub is TDMA coordinator, broadcasts sync beacon in Slot 3
- All nodes synchronize to hub beacon on power-up
- Slot 0 starts 0ms after sync beacon
- Maximum clock drift tolerance: ±500µs (nodes re-sync every frame)

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
| SYNC | 2 bytes | 0x0C 0x4B ("CK") |
| LEN | 1 byte | Length from SRC to end of PAYLOAD |
| SRC | 1 byte | Source node address |
| DST | 1 byte | Destination node address (0xFF = broadcast) |
| TYPE | 1 byte | Packet type identifier |
| PAYLOAD | 0-50 bytes | Type-specific data |
| CRC16 | 2 bytes | CCITT CRC16 over LEN+SRC+DST+TYPE+PAYLOAD |

### Node Addresses

| Address | Node |
|---------|------|
| 0x00 | Hub |
| 0x01 | Crib Pad |
| 0x02 | Nursery Monitor |
| 0x03 | Feeding Station |
| 0xFF | Broadcast |

### Packet Types

| Type | Code | Payload Size | Description |
|------|------|--------------|-------------|
| CRIB_DATA | 0x01 | 24 bytes | Crib pad sensor readings |
| NURSERY_DATA | 0x02 | 30 bytes | Nursery monitor readings |
| FEEDING_DATA | 0x03 | 20 bytes | Feeding station readings |
| CRY_EVENT | 0x04 | 12 bytes | Cry classification event |
| BREATHING_ALERT | 0x05 | 10 bytes | Breathing safety alert |
| COMMAND | 0x06 | 10 bytes | Command to node |
| ACK | 0x07 | 2 bytes | Acknowledgment |
| OTA_BLOCK | 0x08 | 50 bytes | OTA firmware chunk |
| SLEEP_STAGE | 0x09 | 8 bytes | Sleep stage update |
| ENV_EVENT | 0x0A | 8 bytes | Environment event |
| HEARTBEAT | 0x0B | 4 bytes | Keep-alive |

## Data Structures

### CRIB_DATA (0x01) — 24 bytes

| Offset | Size | Field | Type | Range | Unit |
|--------|------|-------|------|-------|------|
| 0 | 1 | breath_rate | uint8 | 0-120 | Breaths per minute |
| 1 | 1 | breath_regularity | uint8 | 0-100 | Regularity index |
| 2 | 1 | movement_score | uint8 | 0-255 | Movement intensity |
| 3 | 1 | position | uint8 | 0-5 | Position (POS_*) |
| 4 | 2 | temp_c_x10 | int16 | -200-500 | Temperature ×10 °C |
| 6 | 1 | wetness_flag | uint8 | 0-1 | Wetness detected |
| 7 | 1 | wetness_level | uint8 | 0-255 | Conductivity level |
| 8 | 2 | breath_apnea_count | uint16 | 0-65535 | Apnea events in last minute |
| 10 | 2 | movement_epochs | uint16 | 0-65535 | Movement count |
| 12 | 1 | alert_level | uint8 | 0-4 | Alert level |
| 13 | 1 | battery_pct | uint8 | 0-100 | Battery percentage |
| 14 | 1 | signal_strength | uint8 | 0-255 | RSSI |
| 15-23 | 8 | fsr_raw[4] | uint16×4 | 0-4095 | FSR peak values |
| 21-23 | 3 | reserved | - | - | Future use |

### NURSERY_DATA (0x02) — 30 bytes

| Offset | Size | Field | Type | Range | Unit |
|--------|------|-------|------|-------|------|
| 0 | 1 | cry_type | uint8 | 0-5 | CRY_* classification |
| 1 | 1 | cry_confidence | uint8 | 0-255 | Confidence |
| 2 | 1 | cry_intensity | uint8 | 0-255 | Sound intensity |
| 3 | 2 | room_temp_c_x10 | int16 | -200-500 | Room temp ×10 °C |
| 5 | 2 | room_humidity_x10 | uint16 | 0-1000 | Humidity ×10 % |
| 7 | 2 | co2_ppm | uint16 | 400-5000 | CO2 ppm |
| 9 | 2 | voc_index | uint16 | 0-500 | VOC index |
| 11 | 2 | light_lux | uint16 | 0-120000 | Ambient light lux |
| 13 | 1 | noise_level_db | uint8 | 0-120 | Background noise dB |
| 14 | 1 | ir_active | uint8 | 0-1 | IR LEDs on |
| 15 | 1 | camera_ready | uint8 | 0-1 | Camera module ready |
| 16 | 1 | baby_present | uint8 | 0-1 | Baby detected |
| 17 | 1 | alert_level | uint8 | 0-4 | Alert level |
| 18 | 1 | battery_pct | uint8 | 0-100 | Battery percentage |
| 19 | 1 | signal_strength | uint8 | 0-255 | RSSI |
| 20 | 1 | sound_type_playing | uint8 | 1-7 | Currently playing sound |
| 21 | 2 | sound_duration_s | uint16 | 0-65535 | Sound duration |
| 22-29 | 10 | reserved | - | - | Future use |

### FEEDING_DATA (0x03) — 20 bytes

| Offset | Size | Field | Type | Range | Unit |
|--------|------|-------|------|-------|------|
| 0 | 1 | feeding_state | uint8 | 0-4 | FEED_* state |
| 1 | 2 | bottle_temp_c_x10 | int16 | -200-500 | Bottle temp ×10 °C |
| 3 | 2 | target_temp_c_x10 | int16 | -200-500 | Target temp ×10 °C |
| 5 | 2 | weight_mg | uint16 | 0-5000 | Current weight mg |
| 7 | 2 | start_weight_mg | uint16 | 0-5000 | Start weight mg |
| 9 | 2 | volume_consumed_ml | uint16 | 0-500 | Volume consumed ml |
| 11 | 2 | feeding_duration_s | uint16 | 0-3600 | Duration seconds |
| 13 | 1 | heater_pct | uint8 | 0-100 | Heater power % |
| 14 | 1 | uv_turbidity | uint8 | 0-255 | Milk turbidity |
| 15 | 1 | battery_pct | uint8 | 0-100 | Battery percentage |
| 16 | 1 | signal_strength | uint8 | 0-255 | RSSI |
| 17 | 1 | scale_calibrated | uint8 | 0-1 | Scale calibration state |
| 18-19 | 2 | reserved | - | - | Future use |

### BREATHING_ALERT (0x05) — 10 bytes

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 1 | alert_level | uint8 | ALERT_* level |
| 1 | 1 | breath_rate | uint8 | Current breath rate |
| 2 | 2 | apnea_duration_ms | uint16 | Duration of apnea event |
| 4 | 2 | time_since_breath | uint16 | ms since last breath |
| 6 | 1 | position | uint8 | Current position |
| 7 | 1 | movement_score | uint8 | Movement intensity |
| 8 | 1 | source_node | uint8 | Which node detected |
| 9 | 2 | timestamp_ms | uint16 | Hub-relative timestamp |

## Reliability

### ACK/NACK

- Every data packet expects an ACK in Slot 4
- If no ACK received, node retransmits in next frame's Slot 4
- Maximum 3 retransmissions before reporting link failure

### Breathing Alert Override

- BREATHING_ALERT packet uses broadcast address (0xFF)
- All nodes immediately stop normal TDMA and relay the alert
- Hub forwards alert to cloud via WiFi + push notification
- Normal TDMA resumes after 5 seconds of no alert packets

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