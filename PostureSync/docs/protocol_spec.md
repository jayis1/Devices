# PostureSync Protocol Specification

## Sub-GHz 868 MHz TDMA Mesh Protocol

### Physical Layer
- **Frequency:** 868 MHz (EU) / 915 MHz (US)
- **Modulation:** LoRa (SX1262) or FSK
- **Bandwidth:** 125 kHz (LoRa) / 200 kHz (FSK)
- **Output power:** +22 dBm max
- **Sensitivity:** -137 dBm
- **Range:** 2 km line-of-sight, 200 m indoor

### TDMA Superframe (1000 ms)
```
├── Beacon (20 ms)        — Hub broadcasts sync + slot assignments
├── Slot 0 (50 ms)        — Chair Pad TX
├── Slot 1 (50 ms)        — Desk Sentinel TX
├── Slot 2-9 (50 ms ea)   — Reserved for future nodes
├── Slot 10-18 (50 ms ea) — Retransmission slots (mesh relay)
└── Idle (remaining)       — Energy saving
```

### Frame Format (48 bytes)
| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| Preamble | 0 | 4 | 0xAA × 4 |
| Sync | 4 | 2 | 0x2D 0xD4 |
| Length | 6 | 1 | Payload length (0-32) |
| SrcID | 7 | 2 | Source node ID |
| DstID | 9 | 2 | Destination (0xFFFF = broadcast) |
| MsgType | 11 | 1 | Message type |
| SeqNum | 12 | 2 | Sequence number |
| Payload | 14 | 32 | Message-specific data |
| CRC16 | 46 | 2 | CRC-16/CCITT |

### Message Types
| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x01 | BEACON | Hub→All | TDMA sync + slot assignment |
| 0x02 | SENSOR_DATA | Node→Hub | Sensor readings |
| 0x03 | POSTURE_ALERT | Hub→Node | Posture correction command |
| 0x04 | HAPTIC_CMD | Hub→Node | Haptic pattern trigger |
| 0x05 | JOIN_REQ | Node→Hub | Mesh join request |
| 0x06 | JOIN_ACK | Hub→Node | Mesh join acknowledgment |
| 0x07 | HEARTBEAT | Node→Hub | Keepalive |
| 0x08 | OTA_CHUNK | Hub→Node | Firmware update chunk |
| 0x09 | CONFIG | Hub→Node | Configuration update |
| 0x0A | CAL_REQ | Hub→Node | Calibration request |

### Node IDs
| ID | Node |
|----|------|
| 0x0001 | Hub |
| 0x0002 | Spine Band |
| 0x0003 | Posture Garment |
| 0x0004 | Chair Pad |
| 0x0005 | Desk Sentinel |
| 0xFFFF | Broadcast |

## BLE 5.0 GATT Protocol

### PostureSync Service
**UUID:** `0000P5S0-0000-1000-8000-00805F9B34FB`

### Characteristics
| UUID | Name | Properties | Data Format |
|------|------|------------|------------|
| P5S1 | Spine Angle | Notify | 3× float32 (pitch, roll, yaw) + uint8 posture + uint8 hr |
| P5S2 | EMG Data | Notify | 8× uint16 RMS + 3× float32 curvature + uint8 asymmetry |
| P5S3 | PPG Data | Notify | uint8 HR + uint8 HRV + uint8 SpO2 |
| P5S4 | Posture Score | Notify | uint8 score (0-100) |
| P5S5 | Haptic Cmd | Write | uint8 pattern ID |
| P5S6 | Config | Write | Sampling rate, thresholds |
| P5S7 | Battery | Notify | uint8 battery level % |
| P5S8 | Calibration | Write | uint8 calibration command |

### Haptic Patterns
| ID | Pattern | Description |
|----|---------|-------------|
| 0x00 | None | No haptic |
| 0x01 | Single Tap | Info (DRV2605L waveform #1) |
| 0x02 | Double Pulse | Correction (DRV2605L waveform #24) |
| 0x03 | Triple Burst | Warning (DRV2605L waveform #40) |
| 0x04 | Long Buzz | Alert (DRV2605L waveform #14) |
| 0x05 | Wave | Pulsing (DRV2605L waveform #55) |

## Posture Classes
| ID | Name | Description |
|----|------|-------------|
| 0 | Neutral | Proper alignment |
| 1 | Forward Head | Cervical pitch > 15° |
| 2 | Slouching | Lumbar flexion, pitch > 20° |
| 3 | Hyperextension | Pitch < -10° |
| 4 | Lateral Left | Roll > 5° |
| 5 | Lateral Right | Roll < -5° |
| 6 | Kyphotic | Thoracic flexion |
| 7 | Lordotic | Lumbar hyperextension |
| 8 | Scoliotic | Lateral curvature |
| 9 | Anterior Tilt | Pelvic tilt forward |
| 10 | Posterior Tilt | Pelvic tilt backward |
| 11 | Crossed Legs | Asymmetric sitting |