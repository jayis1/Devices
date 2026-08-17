# TremorSync Communication Protocol

## Sub-GHz 868 MHz TDMA Mesh

### Physical Layer
- **Frequency:** 868 MHz (EU/US ISM band)
- **Modulation:** LoRa (SX1262), SF7, BW 125 kHz
- **Power:** +22 dBm max
- **Sensitivity:** -137 dBm
- **Range:** 200 m indoor, 2 km line-of-sight

### TDMA Superframe

```
Superframe (1000 ms):
├── Beacon (20 ms)        — Hub broadcasts sync + slot assignments
├── Slot 0 (50 ms)        — Gait Pod TX
├── Slot 1 (50 ms)        — Med Station TX
├── Slot 2-9 (50 ms ea)   — Reserved (additional Gait Pods for bilateral)
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
| CRC16 | 46 | 2 | CRC-16/CCITT-FALSE |

### Message Types

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | BEACON | Hub→All | Slot assignments |
| 0x02 | SENSOR_DATA | Node→Hub | Node-specific sensor data |
| 0x03 | FOG_WARNING | Hub→Node | Haptic cueing command |
| 0x04 | MED_REMINDER | Node→Hub | Dose event / missed dose |
| 0x05 | JOIN_REQ | Node→Hub | Mesh join request |
| 0x06 | JOIN_ACK | Hub→Node | Mesh join acknowledgment |
| 0x07 | HEARTBEAT | Node→Hub | Keepalive |
| 0x08 | OTA_CHUNK | Hub→Node | Firmware update chunk |
| 0x09 | CONFIG | Hub→Node | Configuration update |
| 0x0A | CAL_REQ | Hub→Node | Calibration request |
| 0x0B | ONOFF_STATE | Hub→All | Current ON/OFF state broadcast |
| 0x0C | FALL_ALERT | Node→Hub | Fall detected — emergency dispatch |

### Node IDs

| ID | Node |
|----|------|
| 0x0001 | Hub |
| 0x0002 | Tremor Band |
| 0x0003 | Gait Pod |
| 0x0004 | Voice Node |
| 0x0005 | Med Station |
| 0xFFFF | Broadcast |

### Sensor Data Payloads

#### Tremor Payload (32 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | tremor_class | uint8 | 0-3 |
| 1 | tremor_amplitude | float32 | m/s² RMS |
| 5 | bradykinesia_idx | float32 | 0-100 |
| 9 | onoff_state | uint8 | 0=OFF, 1=ON, 2=transition |
| 10 | hr | uint8 | bpm |
| 11 | battery_pct | uint8 | 0-100 |
| 12 | reserved | 20 bytes | |

#### Gait Payload (32 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | stride_length | float32 | meters |
| 4 | cadence | float32 | steps/min |
| 8 | freeze_index | float32 | 0-1 |
| 12 | fog_detected | uint8 | bool |
| 13 | festination | uint8 | bool |
| 14 | battery_pct | uint8 | 0-100 |
| 15 | reserved | 17 bytes | |

#### Voice Payload (32 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | speech_class | uint8 | 0-4 |
| 1 | hypophonia_score | float32 | 0-100 |
| 5 | f0_mean | float32 | Hz |
| 9 | f0_std | float32 | Hz |
| 13 | swallow_event | uint8 | 0=none, 1=normal, 2=prolonged, 3=cough |
| 14 | battery_pct | uint8 | 0-100 |
| 15 | reserved | 17 bytes | |

#### Med Payload (32 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | dose_taken | uint8 | bool |
| 1 | pill_weight_mg | uint8 | verified weight |
| 2 | minutes_since_dose | uint16 | min |
| 4 | onoff_state | uint8 | 0-2 |
| 5 | battery_pct | uint8 | 0-100 |
| 6 | reserved | 26 bytes | |

## BLE 5.0 GATT

### Service UUID
`0000TS00-0000-1000-8000-00805F9B34FB`

### Characteristics

| UUID | Name | Properties | Description |
|------|------|------------|-------------|
| TS01 | Tremor Data | Notify | tremor_class + amplitude |
| TS02 | IMU Stream | Notify | 200 Hz IMU (compressed) |
| TS03 | PPG Data | Notify | HR + HRV + SpO₂ |
| TS04 | Bradykinesia | Notify | Bradykinesia index |
| TS05 | Haptic Cmd | Write | Trigger cueing pattern |
| TS06 | Voice Data | Notify | Hypophonia + swallow events |
| TS07 | Config | Write | Sampling rate, thresholds |
| TS08 | Battery | Notify | Battery level % |
| TS09 | Calibration | Write | Trigger calibration |
| TS0A | ONOFF State | Notify | Current ON/OFF state |

### Haptic Patterns

| Code | Pattern | Use |
|------|---------|-----|
| 0x00 | None | Stop |
| 0x01 | Single tap | Med reminder |
| 0x02 | Double pulse | FOG warning |
| 0x03 | Triple burst | Fall alert |
| 0x04 | Metronome 60 BPM | FOG cueing |
| 0x05 | Metronome 80 BPM | FOG cueing |
| 0x06 | Metronome 100 BPM | FOG cueing |
| 0x07 | Metronome 120 BPM | FOG cueing |
| 0x08 | Long buzz | General alert |