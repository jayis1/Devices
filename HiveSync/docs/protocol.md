# HiveSync — Sub-GHz Protocol Specification

Version: 1.0

## Physical Layer

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915.0 MHz (US) |
| Modulation | GFSK |
| Data Rate | 250 kbps |
| TX Power | +12 dBm (CC1101 max) |
| Channel BW | 200 kHz |
| Coding | NRZ with preamble |
| Range | 500m+ LOS |

## Frame Format

```
┌──────────┬──────┬──────┬───────┬──────┬──────────┬─────────┬──────┐
│ PREAMBLE │ SYNC │ SRC  │ DST   │ TYPE │ PAYLOAD  │ CRC-16  │ RSSI │
│ 4 bytes  │2 byte│2 byte│2 byte │1 byte│ N bytes  │2 bytes  │1 byte│
└──────────┴──────┴──────┴───────┴──────┴──────────┴─────────┴──────┘
```

- **PREAMBLE**: `0xAA 0x55 0xAA 0x55`
- **SYNC**: `0xD3 0x91` (unique sync word)
- **SRC**: Source node ID (little-endian)
- **DST**: Destination node ID (0x0000 = gateway, 0xFFFF = broadcast)
- **TYPE**: Message type (see below)
- **PAYLOAD**: Variable length, type-dependent
- **CRC-16**: CRC-16-CCITT over PREAMBLE through PAYLOAD
- **RSSI**: Appended by receiver (CC1101)

## Message Types

| Type | Code | Direction | Payload |
|------|------|-----------|---------|
| BEACON | 0x01 | Gateway → All | Slot assignments, network params |
| DATA | 0x02 | Node → Gateway | Sensor readings (48 bytes) |
| AUDIO_FEATURES | 0x03 | Node → Gateway | FFT spectral features (24 bytes) |
| WEIGHT_DELTA | 0x04 | Node → Gateway | Weight change report (8 bytes) |
| COMMAND | 0x05 | Gateway → Node | Action command |
| IMAGE_THUMB | 0x06 | Entrance → Gateway | Compressed image thumbnail |
| OTA_BLOCK | 0x07 | Gateway → Node | Firmware update chunk |
| ACK | 0x08 | Both | Acknowledgment |
| ALARM | 0x09 | Node → Gateway | Urgent alert |
| FEEDER_STATUS | 0x0A | Feeder → Gateway | Feeder state report |

## TDMA Schedule

60-second frame, 2-second slots:

| Slot | Time | Usage |
|------|------|-------|
| 0 | 0.0–2.0s | Gateway beacon |
| 1–30 | 2.0–62.0s | Node data uploads |
| 31–45 | 62.0–92.0s | Mesh relay |
| 46–59 | 92.0–120.0s | ALOHA contention |

### Beacon Payload (8 bytes)

| Byte | Field | Description |
|------|-------|-------------|
| 0 | frame_num | Frame counter (modulo 256) |
| 1 | max_nodes | Max nodes supported |
| 2 | slot_start | First available data slot |
| 3 | network_id | Network identifier |
| 4–7 | timestamp | Unix time (seconds) |

### DATA Payload (48 bytes)

| Offset | Size | Field | Type |
|--------|------|-------|------|
| 0 | 4 | temp_brood_c | float32 |
| 4 | 4 | temp_top_c | float32 |
| 8 | 4 | temp_entrance_c | float32 |
| 12 | 4 | humidity_pct | float32 |
| 16 | 4 | weight_kg | float32 |
| 20 | 4 | weight_delta_g | float32 |
| 24 | 4 | accel_rms_mg | float32 |
| 28 | 4 | battery_mv | float32 |
| 32 | 4 | spectral_centroid_hz | float32 |
| 36 | 4 | peak_freq_hz | float32 |
| 40 | 4 | peak_amplitude_db | float32 |
| 44 | 4 | spectral_bandwidth_hz | float32 |

### COMMAND Payload (7 bytes)

| Offset | Size | Field | Type |
|--------|------|-------|------|
| 0 | 1 | cmd_id | uint8 |
| 1 | 2 | param_u16 | uint16 |
| 3 | 4 | param_f32 | float32 |

Command IDs:
- 0x01: DISPENSE_SYRUP (param_u16 = mL)
- 0x02: ADVANCE_PATTY (param_f32 = mm)
- 0x03: VALVE_OPEN
- 0x04: VALVE_CLOSE
- 0x05: SET_INTERVAL (param_u16 = seconds)
- 0x06: OTA_START
- 0x07: REBOOT

## Mesh Relay

Nodes that can hear both the transmitting node and the gateway serve as relay hops. Relay nodes listen during slots 31–45 and retransmit packets addressed to 0x0000 (gateway) if the source RSSI is below threshold (-85 dBm) and the CRC is valid.

Relay nodes increment a hop counter in the ACK payload to prevent infinite relay loops. Max hops: 3.

## Encryption

All payloads are encrypted with AES-128-CCM using a network-wide key provisioned during pairing. The nonce is constructed from the frame counter + source ID.