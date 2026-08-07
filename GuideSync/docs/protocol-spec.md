# GuideSync — Protocol Specification

## Physical & Link Layer

- **Band:** 2.4 GHz BLE 5.0 (all nodes)
- **Topology:** Star network — Vision Hub as central, glasses/cane/band as peripherals
- **Nav Beacons:** BLE advertising only (broadcast, no connection)
- **Connection interval:** 50 ms (glasses), 200 ms (cane, band), 500 ms advertising (beacons)
- **Encryption:** AES-128-CCM (BLE LE Secure Connection, per-node LTK)
- **Range:** 10 m indoor, 30 m LOS
- **Max nodes:** 3 peripherals + 32 beacons = 35

## Message Format

All BLE GATT notifications use a compact binary protocol:

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │
│ 0x47 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┘
```

- **Sync:** `0x47 0x53` = "GS" (GuideSync)
- **Src:** Source node ID (0=Hub, 1=Glasses, 2=Cane, 3=Band, 4+=Beacons)
- **Dst:** Destination (0=Hub, 0xFF=Broadcast)
- **MsgType:** Message type (see below)
- **MsgId:** 16-bit sequence number
- **Payload:** Variable length (0-240 bytes)

Note: BLE link layer provides CRC + AES-128-CCM encryption, so no
application-layer CRC is needed.

## Message Types

| Type | Name | Direction | Payload Size |
|------|------|-----------|--------------|
| 0x01 | JOIN_REQ | Node→Hub | 6 bytes |
| 0x02 | JOIN_ACK | Hub→Node | 2 bytes |
| 0x03 | TELEMETRY | Node→Hub | 12-31 bytes |
| 0x04 | COMMAND | Hub→Node | 1+N bytes |
| 0x05 | CMD_ACK | Node→Hub | 2 bytes |
| 0x06 | ALERT | Node→Hub | 2+N bytes |
| 0x07 | OTA_BLOCK | Hub→Node | variable |
| 0x08 | OTA_ACK | Node→Hub | 4 bytes |
| 0x09 | HEARTBEAT | Node→Hub | 2 bytes |
| 0x0A | NAV_UPDATE | Hub→Band | 6 bytes |
| 0x0B | SCENE_DESC | Glasses→Hub | variable |
| 0x0C | OCR_REQUEST | Glasses→Hub | variable |
| 0x0D | OCR_RESULT | Hub→Glasses | 1+N bytes |
| 0x0E | FALL_ALERT | Band→Hub | 8 bytes |
| 0x0F | SOS_ALERT | Band→Hub | 4 bytes |
| 0x10 | BEACON_SCAN | Glasses/Band→Hub | 4+5×N |
| 0x11 | NAV_DEST | Hub→Glasses | variable |
| 0x12 | CALIBRATION | Hub→Node | variable |
| 0x13 | CALIB_ACK | Node→Hub | 2 bytes |
| 0x14 | TIME_SYNC | Hub→All | 4 bytes |

## Telemetry Payloads

### Glasses Telemetry (31 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x01 (GLASSES) |
| 1 | battery_v | 1 | Battery voltage (×0.01V) |
| 2 | head_pitch | 1 | Head pitch (degrees, signed) |
| 3 | head_roll | 1 | Head roll (degrees, signed) |
| 4 | head_yaw | 1 | Head yaw (degrees, signed) |
| 5 | obstacle_class | 1 | SceneNet nearest obstacle class |
| 6 | obstacle_dist_dm | 1 | Obstacle distance (decimeters) |
| 7 | obstacle_dir | 1 | Obstacle direction (0=center) |
| 8 | scene_obj_count | 1 | Objects in scene |
| 9 | primary_obj_class | 1 | Primary object class |
| 10 | primary_obj_dist_dm | 1 | Primary object distance (dm) |
| 11 | crosswalk_detected | 1 | 0=no, 1=yes |
| 12 | signal_state | 1 | 0=none, 1=walk, 2=don't, 3=countdown |
| 13 | countdown_sec | 1 | Countdown seconds (if signal_state=3) |
| 14 | tof_min_dist_dm | 1 | ToF grid minimum distance (dm) |
| 15 | tof_hazard_flag | 1 | ObstacleNet hazard class (0-5) |
| 16 | audio_vol | 1 | Bone conduction volume (0-100) |
| 17 | bone_conduction_active | 1 | 0=off, 1=on |
| 18-19 | step_count_24h | 2 | Step count (24h) |
| 20 | imu_temp | 1 | IMU temperature (°C, signed) |
| 21-22 | scenenet_ms | 2 | SceneNet inference time (ms) |
| 23-24 | crosswalknet_ms | 2 | CrosswalkNet inference time (ms) |
| 25-26 | free_heap | 2 | Free heap (bytes) |
| 27 | ble_rssi | 1 | BLE RSSI (dBm, signed) |
| 28-29 | uptime_min | 2 | Uptime (minutes) |
| 30 | tof_valid_zones | 1 | Valid ToF zones (0-64) |

### Cane Telemetry (14 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x02 (CANE) |
| 1 | battery_v | 1 | Battery voltage (×0.01V) |
| 2 | us_dist_dm | 1 | Ultrasonic distance (dm) |
| 3 | us_valid | 1 | 0=invalid, 1=valid |
| 4 | tof_down_dm | 1 | Downward ToF distance (dm) |
| 5 | dropoff_detected | 1 | 0=no, 1=yes |
| 6 | stair_detected | 1 | 0=no, 1=yes |
| 7-8 | swing_count_24h | 2 | Cane swing count (24h) |
| 9 | imu_temp | 1 | IMU temperature (°C, signed) |
| 10 | haptic_last | 1 | Last haptic pattern ID |
| 11 | haptic_active | 1 | 0=off, 1=on |
| 12 | cane_tilt_deg | 1 | Cane tilt (degrees, signed) |
| 13 | ble_rssi | 1 | BLE RSSI (dBm, signed) |

### Haptic Band Telemetry (13 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x03 (BAND) |
| 1 | battery_v | 1 | Battery voltage (×0.01V) |
| 2 | imu_temp | 1 | IMU temperature (°C, signed) |
| 3-4 | step_count_24h | 2 | Step count (24h) |
| 5 | fall_count_24h | 1 | Fall count (24h) |
| 6 | haptic_last | 1 | Last haptic pattern |
| 7 | nav_direction | 1 | Current nav direction (0-7) |
| 8 | nav_distance_m | 1 | Distance to next waypoint (m) |
| 9 | sos_armed | 1 | 0=disarmed, 1=armed |
| 10 | ble_rssi | 1 | BLE RSSI (dBm, signed) |
| 11-12 | uptime_min | 2 | Uptime (minutes) |

## Alert Types

| Type | Name | Severity | Description |
|------|------|----------|-------------|
| 0x01 | LOW_BATTERY | WARNING | Battery below threshold |
| 0x02 | OBSTACLE_CRIT | CRITICAL | Obstacle <1 m (dual-confirmed) |
| 0x03 | OBSTACLE_WARN | WARNING | Obstacle 1-2 m |
| 0x04 | DROP_OFF | CRITICAL | Drop-off/stair edge detected |
| 0x05 | STAIRS | WARNING | Stair transition detected |
| 0x06 | FALL | EMERGENCY | Fall detected (FallNet confirmed) |
| 0x07 | SOS | EMERGENCY | SOS button activated |
| 0x08 | CROSSWALK_WALK | INFO | Walk signal detected |
| 0x09 | CROSSWALK_DONT | WARNING | Don't walk signal detected |
| 0x0A | NODE_OFFLINE | WARNING | Node lost BLE connection |
| 0x0B | SENSOR_ANOMALY | WARNING | Sensor fault detected |
| 0x0C | ARRIVED | INFO | Navigation destination reached |
| 0x0D | TEXT_READ | INFO | Text successfully read (OCR) |
| 0x0E | FACE_RECOGNIZED | INFO | Familiar face recognized |

## Navigation Haptic Patterns

| Direction | DRV2605L Sequence | Description |
|-----------|-------------------|-------------|
| Straight | Sharp click ×1 | Continue forward |
| Left | Strong click ×2 | Turn left |
| Right | Sharp click ×3 | Turn right |
| Turn Around | — | U-turn |
| Stop | Long hum 500ms | Stop / obstacle |
| Arrived | Ascending: soft→strong | Destination reached |
| Upstairs | Ascending ×4 | Stairs up |
| Downstairs | Descending ×4 | Stairs down |
| Fall | Strong rumble ×5 | Fall detected |
| SOS confirm | Descending: strong→soft | SOS dispatched |

## Beacon Advertisement Format

Nav beacons broadcast BLE manufacturer-specific data:

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0-1 | Company ID | 2 | 0x0059 (Nordic) |
| 2-5 | UUID prefix | 4 | 0x47, 0x53, 0xBE, 0xAC |
| 6-7 | Beacon ID | 2 | Unique 16-bit ID |
| 8 | Battery | 1 | Battery voltage (×0.01V) |

Beacons advertise every 500 ms. Glasses and band scan for 2 seconds
to build RSSI fingerprints for NavNet positioning.