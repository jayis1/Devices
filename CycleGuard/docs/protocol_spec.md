# CycleGuard Communication Protocol

## Sub-GHz 868 MHz TDMA Mesh

### Physical Layer
- **Frequency:** 868 MHz (EU/US ISM band)
- **Modulation:** LoRa (SX1262), SF7, BW 125 kHz
- **Power:** +22 dBm max
- **Sensitivity:** -137 dBm
- **Range:** 500 m urban, 2 km line-of-sight

### TDMA Superframe

```
Superframe (1000 ms):
├── Beacon (20 ms)        — Hub broadcasts sync + slot assignments
├── Slot 0 (50 ms)        — Smart Lock TX
├── Slot 1-10 (50 ms ea)  — Reserved (additional locks)
├── Slot 11-18 (50 ms ea) — Retransmission slots (mesh relay)
└── Idle (remaining)       — Energy saving
```

### Frame Format (48 bytes)

| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| Preamble | 0 | 4 | 0x55 × 4 |
| Sync | 4 | 2 | 0x3C 0xD2 |
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
| 0x03 | CRASH_ALERT | Helmet→Hub | Crash detected — emergency dispatch |
| 0x04 | LOCK_CMD | Hub→Lock | Arm/disarm command |
| 0x05 | JOIN_REQ | Node→Hub | Mesh join request |
| 0x06 | JOIN_ACK | Hub→Node | Mesh join acknowledgment |
| 0x07 | HEARTBEAT | Node→Hub | Keepalive |
| 0x08 | OTA_CHUNK | Hub→Node | Firmware update chunk |
| 0x09 | CONFIG | Hub→Node | Configuration update |
| 0x0A | CAL_REQ | Hub→Node | Calibration request |
| 0x0B | PROXIMITY_WARN | Hub→Helmet | Blind spot warning + haptic pattern |
| 0x0C | TURN_SIGNAL | Hub→Light | Turn signal command |
| 0x0D | THEFT_ALERT | Lock→Hub | Theft in progress |
| 0x0E | LIGHT_CMD | Hub→Light | Light mode/brightness command |
| 0x0F | TIRE_PRESSURE | Sensor→Hub | TPMS pressure data |

### Node IDs

| ID | Node |
|----|------|
| 0x0001 | Hub |
| 0x0002 | Smart Helmet |
| 0x0003 | Smart Light |
| 0x0004 | Bike Sensor |
| 0x0005 | Smart Lock |
| 0xFFFF | Broadcast |

### Sensor Data Payloads

#### Helmet Payload (32 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | crash_class | uint8 | 0-3 (normal/pothole/near-miss/crash) |
| 1 | impact_g | float32 | Peak acceleration in g |
| 5 | rot_velocity | float32 | Peak rotational velocity deg/s |
| 9 | horn_detected | uint8 | bool |
| 10 | siren_detected | uint8 | bool |
| 11 | hr | uint8 | bpm |
| 12 | battery_pct | uint8 | 0-100 |
| 13 | reserved | 19 bytes | |

#### Bike Sensor Payload (32 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | speed_kmh | float32 | Wheel speed km/h |
| 4 | cadence_rpm | uint8 | Crank cadence RPM |
| 5 | tire_pressure_psi | float32 | TPMS pressure PSI |
| 9 | tire_temp_c | float32 | TPMS temperature °C |
| 13 | battery_pct | uint8 | 0-100 |
| 14 | reserved | 18 bytes | |

#### Lock Payload (32 bytes)
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | lock_state | uint8 | 0-4 (disarmed/armed/tamper/alarm/tracking) |
| 1 | gps_lat_e7 | int32 | Latitude × 1e7 |
| 5 | gps_lon_e7 | int32 | Longitude × 1e7 |
| 9 | tamper_count | uint8 | Tamper events since arm |
| 10 | load_cell_kg | uint16 | Current prying force kg |
| 12 | battery_pct | uint8 | 0-100 |
| 13 | reserved | 19 bytes | |

## BLE 5.0 GATT

### Service UUID
`0000CG00-0000-1000-8000-00805F9B34FB`

### Characteristics

| UUID | Name | Properties | Description |
|------|------|------------|-------------|
| CG01 | Helmet Data | Notify | crash_class + impact + horn/siren |
| CG02 | IMU Stream | Notify | 500 Hz IMU (compressed) |
| CG03 | Light Status | Notify | braking + turn + brightness |
| CG04 | Speed/Cadence | Notify | wheel speed + cadence + TPMS |
| CG05 | Light Cmd | Write | Turn signal / mode / brightness |
| CG06 | Proximity Cmd | Write | Trigger haptic pattern on helmet |
| CG07 | Config | Write | Sampling rate, thresholds |
| CG08 | Battery | Notify | Battery level % |
| CG09 | Calibration | Write | Trigger calibration |
| CG0A | Crash Alert | Notify | Crash detected flag |
| CG0B | Lock Status | Notify | Lock state + GPS |
| CG0C | OTA | Write | Firmware update chunk |

### Haptic Patterns

| Code | Pattern | Use |
|------|---------|-----|
| 0x00 | None | Stop |
| 0x01 | Single tap | Turn approaching |
| 0x02 | Double pulse | Vehicle behind |
| 0x03 | Triple burst | Crash imminent |
| 0x04 | Long buzz | General alert |
| 0x05 | Theft alarm | Theft detected |