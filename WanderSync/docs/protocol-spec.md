# WanderSync — Protocol Specification

## Physical Layer

- **Band:** 868 MHz Sub-GHz (SX1262)
- **Modulation:** LoRa, SF7, BW 125 kHz, CR 4/5
- **TX Power:** +22 dBm
- **Sensitivity:** -107 dBm
- **Range:** 100 m indoor (3+ walls), 2+ km LOS
- **Sync Word:** 0x3445 (private network)

## TDMA Mesh

- **Topology:** Hub = coordinator, all nodes = mesh relays
- **Slots:** 16 × 250 ms = 4 s cycle
  - Slot 0: Hub
  - Slots 1–14: Nodes (doors, rooms, band, voice)
  - Slot 15: Priority (emergency preemption)
- **Self-healing:** If a node dies, neighbors relay messages
- **Emergency:** WANDER_ALERT/FALL_ALERT/SOS bypass TDMA (3× immediate TX, <2 s latency)

## Encryption

- AES-128-CTR (application layer, per-node key)
- Nonce: 8 bytes, incremented per message
- Key distribution: Per-node key provisioned during manufacturing or OTA

## Message Format

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16(2) │
│ 0x57 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

- **Sync:** `0x57 0x53` = "WS" (WanderSync)
- **Src:** Source node ID (0x00 = Hub, 0x01–0x1E = nodes)
- **Dst:** Destination node ID (0xFF = broadcast)
- **MsgType:** Message type (see below)
- **MsgId:** 16-bit message counter (for deduplication)
- **Payload:** 0–240 bytes (message-specific)
- **CRC:** CRC-16-CCITT over bytes [2..len-3] (src through payload)

## Message Types

| Type | Name | Direction | Priority | Payload Size |
|------|------|-----------|----------|-------------|
| 0x01 | JOIN_REQ | Node→Hub | Normal | 4 |
| 0x02 | JOIN_ACK | Hub→Node | Normal | 1 |
| 0x03 | TELEMETRY | Node→Hub | Normal | 10–28 |
| 0x04 | COMMAND | Hub→Node | Normal | 1+ |
| 0x05 | CMD_ACK | Node→Hub | Normal | 2 |
| 0x06 | WANDER_ALERT | Band→Hub | **EMERGENCY** | 16 |
| 0x07 | FALL_ALERT | Band→Hub | **EMERGENCY** | 16 |
| 0x08 | SOS_ALERT | Band→Hub | **EMERGENCY** | 16 |
| 0x09 | DOOR_ALERT | Door→Hub | High | 3 |
| 0x0A | DOOR_LOCK_CMD | Hub→Door | High | 6 |
| 0x0B | DOOR_LOCK_ACK | Door→Hub | High | 3 |
| 0x0C | REMINDER_TRIG | Hub→Voice | Normal | 8 |
| 0x0D | REMINDER_ACK | Voice→Hub | Normal | 2 |
| 0x0E | ADL_UPDATE | Room→Hub | Normal | 5 |
| 0x0F | ANOMALY_ALERT | Hub→Cloud | High | variable |
| 0x10 | EMERGENCY_DISP | Hub→Cloud | **EMERGENCY** | variable |
| 0x11 | OTA_BLOCK | Hub→Node | Low | 64+ |
| 0x12 | OTA_ACK | Node→Hub | Low | 4 |
| 0x13 | HEARTBEAT | Node→Hub | Normal | 4 |
| 0x14 | GEOFENCE_UPD | Hub→Band | Normal | 12 |
| 0x15 | GEOFENCE_ACK | Band→Hub | Normal | 0 |
| 0x16 | TIME_SYNC | Hub→All | Normal | 4 |
| 0x17 | SILENCE | Hub→All | Normal | 0 |
| 0x18 | TEST_ALARM | Hub→All | Normal | 0 |
| 0x19 | CALIBRATION | Hub→Node | Normal | variable |
| 0x1A | CALIB_ACK | Node→Hub | Normal | 2 |
| 0x1B | TAMPER_ALERT | Door→Hub | High | 3 |
| 0x1C | BAND_REMOVED | Band→Hub | High | 16 |
| 0x1D | REMINDER_SCHED | Hub→Voice | Normal | 24×5 |

## Telemetry Payloads

### Band Telemetry (28 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x01 (BAND) |
| 1 | battery_v | 1 | Battery voltage × 0.01V |
| 2 | gps_lat_e7 | 4 | Latitude × 1e7 (signed) |
| 6 | gps_lon_e7 | 4 | Longitude × 1e7 (signed) |
| 10 | gps_fix | 1 | 0=no, 1=fix, 2=estimated |
| 11 | activity_class | 1 | 0=sit, 1=walk, 2=lie, 3=stand, 4=fidget, 5=fall |
| 12 | wander_risk | 1 | WanderNet risk (0-100) |
| 13 | hr_bpm | 1 | Heart rate |
| 14 | hrv_ms | 2 | HRV (RMSSD) |
| 16 | steps | 2 | Steps since last telemetry |
| 18 | geofence_status | 1 | 0=inside, 1=outside, 2=near |
| 19 | distance_home_m | 2 | Distance from home center |
| 21 | band_on_wrist | 1 | 0=removed, 1=worn |
| 22 | free_heap | 2 | Free heap (bytes) |
| 24 | rssi | 1 | Sub-GHz RSSI (dBm) |
| 25 | uptime_min | 2 | Uptime (minutes) |
| 27 | sos_pressed | 1 | 0=no, 1=pressed since last |

### Door Telemetry (10 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x02 (DOOR) |
| 1 | battery_v | 1 | × 0.01V |
| 2 | door_id | 1 | Door identifier |
| 3 | door_state | 1 | 0=closed, 1=open |
| 4 | lock_state | 1 | 0=unlocked, 1=locked, 2=failed |
| 5 | tamper | 1 | 0=ok, 1=tampered |
| 6 | band_proximity | 1 | 0=absent, 1=present |
| 7 | rssi | 1 | dBm |
| 8 | uptime_min | 2 | Uptime |

### Room Telemetry (16 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x03 (ROOM) |
| 1 | battery_v | 1 | × 0.01V |
| 2 | room_id | 1 | Room identifier |
| 3 | presence | 1 | 0=empty, 1=present |
| 4 | activity_class | 1 | ADLNet 0-7 |
| 5 | activity_conf | 1 | 0-100% |
| 6 | motion_level | 1 | 0-255 mmWave |
| 7 | range_m_x2 | 1 | Distance × 0.5 m |
| 8 | pir_triggered | 1 | 0=no, 1=yes |
| 9 | adlnet_ms | 2 | Inference time |
| 11 | free_heap | 2 | bytes |
| 13 | rssi | 1 | dBm |
| 14 | uptime_min | 2 | minutes |

### Voice Telemetry (12 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | subtype | 1 | 0x04 (VOICE) |
| 1 | battery_v | 1 | × 0.01V |
| 2 | speaker_active | 1 | 0=off, 1=playing |
| 3 | last_keyword | 1 | 0-10, 0xFF=none |
| 4 | reminders_24h | 1 | Count in last 24h |
| 5 | ack_rate | 1 | 0-100% |
| 6 | free_heap | 2 | bytes |
| 8 | rssi | 1 | dBm |
| 9 | uptime_min | 2 | minutes |

## Wander Alert Payload (16 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | alert_type | 1 | 1=wander, 2=fall, 3=sos, 4=band_removed |
| 1 | gps_lat_e7 | 4 | Latitude × 1e7 |
| 5 | gps_lon_e7 | 4 | Longitude × 1e7 |
| 9 | wander_risk | 1 | 0-100 |
| 10 | activity_class | 1 | Current activity |
| 11 | geofence_status | 1 | 0=inside, 1=outside, 2=near |
| 12 | battery_v | 1 | × 0.01V |
| 13 | impact_g_x10 | 1 | Fall impact × 0.1g |
| 14 | band_on_wrist | 1 | 0=removed, 1=worn |
| 15 | hr_bpm | 1 | Heart rate at alert |

## Door Lock Command (6 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | door_id | 1 | 0xFF = all doors |
| 1 | action | 1 | 0=unlock, 1=lock, 2=schedule, 3=emergency_unlock |
| 2 | schedule_id | 1 | 0=normal, 1=night, 2=override |
| 3 | duration_s | 2 | Override duration (0=permanent) |
| 5 | priority | 1 | 0=normal, 1=high, 2=emergency |

## Reminder Trigger (8 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | reminder_id | 1 | Unique identifier |
| 1 | clip_index | 1 | W25Q128 flash clip (0-119) |
| 2 | volume | 1 | 0-100% |
| 3 | repeat_count | 1 | 1-3 |
| 4 | repeat_delay | 1 | Seconds between repeats |
| 5 | tone_before | 1 | 0=no, 1=chime |
| 6 | reminder_type | 1 | 0=med, 1=meal, 2=water, 3=appt, 4=orient, 5=custom |
| 7 | ack_timeout | 1 | Seconds before escalation |

## Geofence Update (12 bytes)

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0 | center_lat_e7 | 4 | Geofence center latitude × 1e7 |
| 4 | center_lon_e7 | 4 | Geofence center longitude × 1e7 |
| 8 | radius_m | 2 | Day geofence radius (m) |
| 10 | night_radius_m | 2 | Night geofence radius (m) |
| 12 | night_start_h | 1 | Night start hour (not in struct due to packing) |