# WashWise — Mesh Protocol Specification

## Overview

WashWise uses a custom TDMA (Time Division Multiple Access) protocol over
LoRa 868MHz Sub-GHz radio for reliable, low-latency communication between
laundry room nodes. The protocol is designed for:

- **Fire safety priority**: Dryer fire alerts override normal TDMA
- **Deterministic latency**: Each node has a guaranteed time slot
- **Low power**: Scanner achieves <5µA in deep sleep
- **Reliability**: CRC16 error detection + ACK/retransmit

## Physical Layer

| Parameter | EU (868MHz) | US (915MHz) |
|-----------|-------------|-------------|
| Frequency | 868.0 MHz | 915.0 MHz |
| Modulation | LoRa | LoRa |
| Spreading Factor | SF7 (normal) | SF7 (normal) |
| | SF10 (fire alerts) | SF10 (fire alerts) |
| Bandwidth | 125 kHz | 125 kHz |
| TX Power | +14 dBm (nodes) | +20 dBm (nodes) |
| | +20 dBm (hub) | +27 dBm (hub) |
| Range (indoor) | 30m | 30m |
| Range (long range) | 200m | 500m |
| Coding Rate | 4/5 | 4/5 |
| Preamble | 8 symbols | 8 symbols |
| Sync Word | 0x5A5A | 0x5A5A |

## TDMA Frame Structure

```
Time →  0ms     100ms    200ms    300ms    400ms
        ┌───────┬───────┬───────┬───────┬───────┐
        │ HUB   │WASHER │DRYER  │SCANNER│ CTRL  │
        │ CMD   │ DATA  │ DATA  │ DATA  │ ACK   │
        │Slot 0 │Slot 1 │Slot 2 │Slot 3 │Slot 4│
        └───────┴───────┴───────┴───────┴───────┘

        500ms total frame, repeats continuously

Fire Alert Override:
        ┌───────────────────────────────────────────────┐
        │ FIRE_ALERT broadcast (SF10, long range)       │
        │ Dryer node overrides TDMA, hub halts normal  │
        │ scheduling, activates alarm + cloud relay   │
        └───────────────────────────────────────────────┘
```

### Slot Assignments

| Slot | Node | Duration | Purpose |
|------|------|----------|---------|
| 0 | Hub | 100ms | Sync beacon + commands broadcast |
| 1 | Washer | 100ms | Washer telemetry (vibration, flow, temp, dose) |
| 2 | Dryer | 100ms | Dryer telemetry (SAFETY PRIORITY — fire risk) |
| 3 | Scanner | 100ms | Scan results (when scanner is active) |
| 4 | Any | 100ms | ACK + retransmit + OTA |

### Slot Timing
- Hub is TDMA coordinator, broadcasts sync beacon in Slot 0
- All nodes synchronize to hub beacon on power-up
- Slot 1 starts 100ms after sync beacon
- Maximum clock drift tolerance: ±500µs (nodes re-sync every frame)

## Packet Format

```
┌─────────┬──────────┬──────┬──────┬──────┬──────┬──────────┬───────┐
│PREAMBLE │ SYNC     │ LEN  │ SRC  │ DST  │ TYPE │ PAYLOAD  │ CRC16 │
│ 4 bytes │ 2 bytes  │ 1B   │ 1B   │ 1B   │ 1B   │ 0-50B    │ 2B    │
└─────────┴──────────┴──────┴──────┴──────┴──────┴──────────┴───────┘

Total: 12-62 bytes
Over-the-air time (SF7): ~20-100ms (within 100ms slot)
Over-the-air time (SF10): ~300ms (fire alert, no slot limit)
```

### Fields

| Field | Size | Description |
|-------|------|-------------|
| PREAMBLE | 4 bytes | 0xAA 0xAA 0xAA 0xAA |
| SYNC | 2 bytes | 0x5A 0x5A ("WW") |
| LEN | 1 byte | Payload length |
| SRC | 1 byte | Source node address |
| DST | 1 byte | Destination (0xFF = broadcast) |
| TYPE | 1 byte | Packet type |
| PAYLOAD | 0-50 bytes | Type-specific data |
| CRC16 | 2 bytes | CCITT CRC16 over LEN+SRC+DST+TYPE+PAYLOAD |

### Node Addresses

| Address | Node |
|---------|------|
| 0x00 | Hub |
| 0x01 | Washer |
| 0x02 | Dryer |
| 0x03 | Scanner |
| 0xFF | Broadcast |

### Packet Types

| Type | Code | Payload | Description |
|------|------|---------|-------------|
| WASHER_DATA | 0x01 | 36 bytes | Washer telemetry |
| DRYER_DATA | 0x02 | 30 bytes | Dryer telemetry (fire risk) |
| SCAN_RESULT | 0x03 | 40 bytes | Stain/fabric scan result |
| COMMAND | 0x04 | 18 bytes | Command to node |
| ACK | 0x05 | 2 bytes | Acknowledgment |
| OTA_BLOCK | 0x06 | 50 bytes | OTA firmware chunk |
| CALIBRATION | 0x07 | variable | Sensor calibration |
| FIRE_ALERT | 0x08 | 12 bytes | Critical fire alert (SF10) |
| ENERGY_DATA | 0x09 | 16 bytes | Per-cycle energy/water |
| HEARTBEAT | 0x0A | 4 bytes | Keep-alive |

## Data Structures

### WASHER_DATA (0x01) — 36 bytes

| Offset | Size | Field | Type | Range | Unit |
|--------|------|-------|------|-------|------|
| 0 | 1 | cycle_phase | uint8 | 0-5 | Phase (IDLE/FILL/WASH/RINSE/SPIN/DONE) |
| 1 | 2 | vibration_rms_x10 | uint16 | 0-10000 | milli-g ×10 |
| 3 | 2 | flow_rate_mlmin | uint16 | 0-30000 | mL/min |
| 5 | 2 | total_water_ml | uint16 | 0-65535 | mL this cycle |
| 7 | 2 | water_temp_c_x10 | int16 | -200-1000 | °C ×10 |
| 9 | 2 | ambient_hum_x10 | uint16 | 0-1000 | % ×10 |
| 11 | 1 | motor_state | uint8 | 0-2 | off/on/spin |
| 12 | 2 | current_ma | uint16 | 0-30000 | mA |
| 14 | 2 | detergent_mg | uint16 | 0-65535 | mg dispensed |
| 16 | 2 | reservoir_g_x10 | uint16 | 0-6553 | g ×10 |
| 18 | 1 | fabric_type | uint8 | 0-8 | Fabric class |
| 19 | 1 | imbalance_flag | uint8 | 0-2 | ok/warning/severe |
| 20 | 1 | leak_flag | uint8 | 0-1 | leak suspected |
| 21 | 1 | battery_pct | uint8 | 0-100 | % |
| 22 | 1 | signal_rssi | uint8 | 0-255 | RSSI |
| 23-24 | 2 | reserved | - | - | Future use |

### DRYER_DATA (0x02) — 30 bytes

| Offset | Size | Field | Type | Range | Unit |
|--------|------|-------|------|-------|------|
| 0 | 2 | exhaust_temp_c_x10 | int16 | -200-1200 | °C ×10 |
| 2 | 2 | ambient_temp_c_x10 | int16 | -200-500 | °C ×10 |
| 4 | 2 | diff_pressure_pa | uint16 | 0-1000 | Pa |
| 6 | 2 | exhaust_hum_x10 | uint16 | 0-1000 | % ×10 |
| 8 | 2 | vibration_rms_x10 | uint16 | 0-10000 | milli-g ×10 |
| 10 | 2 | current_ma | uint16 | 0-30000 | mA |
| 12 | 1 | dryer_state | uint8 | 0-4 | off/heating/tumbling/cooling/done |
| 13 | 1 | heating_on | uint8 | 0-1 | heating element active |
| 14 | 1 | fire_risk_score | uint8 | 0-255 | ML risk (0.0-1.0) |
| 15 | 1 | lint_clog_level | uint8 | 0-3 | clean/mild/moderate/severe |
| 16 | 1 | dryness_level | uint8 | 0-3 | wet/damp/dry/over-dry |
| 17 | 1 | alert_level | uint8 | 0-4 | ok/info/warning/critical/emergency |
| 18 | 1 | battery_pct | uint8 | 0-100 | % |
| 19 | 1 | signal_rssi | uint8 | 0-255 | RSSI |
| 20-21 | 2 | reserved | - | - | Future use |

### FIRE_ALERT (0x08) — 12 bytes

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 1 | alert_level | uint8 | 3=critical, 4=emergency |
| 1 | 1 | fire_risk_score | uint8 | 0-255 |
| 2 | 2 | exhaust_temp_c_x10 | int16 | °C ×10 |
| 4 | 2 | diff_pressure_pa | uint16 | Pa |
| 6 | 1 | lint_clog_level | uint8 | 0-3 |
| 7 | 1 | heating_on | uint8 | 0-1 |
| 8 | 2 | timestamp_ms | uint16 | Hub-relative ms |
| 9 | 1 | source_node | uint8 | Which node detected |
| 10 | 1 | reserved | - | - |

### SCAN_RESULT (0x03) — 40 bytes

| Offset | Size | Field | Type | Range | Description |
|--------|------|-------|------|-------|-------------|
| 0 | 1 | fabric_type | uint8 | 0-8 | Fabric class |
| 1 | 1 | fabric_conf | uint8 | 0-255 | Confidence |
| 2 | 1 | stain_type | uint8 | 0-10 | Stain class |
| 3 | 1 | stain_conf | uint8 | 0-255 | Confidence |
| 4 | 2 | wash_temp_c_x10 | int16 | -200-1000 | Recommended temp ×10 |
| 6 | 1 | recommended_cycle | uint8 | 0-4 | normal/delicate/heavy/quick/handwash |
| 7 | 1 | detergent_ml | uint8 | 0-100 | Recommended dose mL |
| 8 | 1 | pre_treat_id | uint8 | 0-10 | Pre-treatment method |
| 9-16 | 8 | care_label | uint8×8 | - | Extracted care symbols |
| 17-18 | 2 | image_id | uint16 | - | Cloud image reference |
| 19 | 1 | battery_pct | uint8 | 0-100 | % |
| 20 | 1 | signal_rssi | uint8 | 0-255 | RSSI |
| 21-22 | 2 | reserved | - | - | Future use |

## Reliability

### ACK/NACK
- Every data packet expects an ACK in Slot 4
- If no ACK received, node retransmits in next frame's Slot 4
- Maximum 3 retransmissions before reporting link failure

### Fire Alert Override
- FIRE_ALERT packet uses broadcast address (0xFF) + SF10
- All nodes immediately stop normal TDMA and relay the alert
- Hub forwards alert to cloud via WiFi + push notification + SMS
- Hub activates local piezo alarm (85 dB)
- Normal TDMA resumes after 5 seconds of no alert packets

### OTA Updates
- Hub distributes firmware blocks in Slot 4 (command slot)
- Nodes store blocks in external flash
- Complete image verified by CRC32 before applying
- Rollback to previous image if new firmware fails CRC check within 30 seconds

## Encryption (Future)
Current protocol is unencrypted for simplicity. Future versions will implement:
- AES-128-CCM encryption of payload
- Per-node session keys derived from ECDH key exchange
- Key rotation every 24 hours