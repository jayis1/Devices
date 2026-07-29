# RehabSync — Protocol Specification

## 1. Message Format

All messages use a binary framed format with CRC-16-CCITT integrity checking and AES-128-CTR encryption.

### Frame Structure

```
┌──────┬──────┬───────┬───────┬──────────┬──────────┬──────┬─────────────┬─────────┐
│ SYNC0│ SYNC1│ SRC_ID│ DST_ID│ MSG_TYPE │ SUBTYPE  │ SEQ  │ PAYLOAD_LEN │ PAYLOAD │ CRC16  │
│ 0x52 │ 0x53 │ 1 byte│ 1 byte│ 1 byte   │ 1 byte   │ 1 byte│ 1 byte      │ 0-240 B │ 2 bytes│
└──────┴──────┴───────┴───────┴──────────┴──────────┴──────┴─────────────┴─────────┘
```

- **SYNC0/SYNC1:** Fixed sync bytes `0x52 0x53` ('R' 'S')
- **SRC_ID:** Source node ID (1-255, 0xFF = broadcast)
- **DST_ID:** Destination node ID (0xFF = broadcast)
- **MSG_TYPE:** Message type (see below)
- **SUBTYPE:** Message subtype (e.g., telemetry source, alert type)
- **SEQ:** Sequence number (0-255, wraps)
- **PAYLOAD_LEN:** Payload length in bytes (0-240)
- **PAYLOAD:** Variable length payload
- **CRC16:** CRC-16-CCITT over header + payload (polynomial 0x1021, init 0xFFFF)

### Total Message Size
- Minimum: 10 bytes (header + CRC, no payload)
- Maximum: 258 bytes (header + 240 payload + CRC)

## 2. Message Types

| Type | Value | Description |
|------|-------|-------------|
| JOIN_REQ | 0x01 | Node join request (to coordinator) |
| JOIN_ACK | 0x02 | Join acknowledgment (from coordinator) |
| TELEMETRY | 0x03 | Periodic telemetry data |
| COMMAND | 0x04 | Command from Hub to node |
| CMD_ACK | 0x05 | Command acknowledgment |
| ALERT | 0x06 | Alert notification |
| OTA_BLOCK | 0x07 | OTA firmware block |
| OTA_ACK | 0x08 | OTA block acknowledgment |
| HEARTBEAT | 0x09 | Periodic heartbeat |
| MESH_RELAY | 0x0A | Mesh relay message |
| TIME_SYNC | 0x0B | Time synchronization |
| CONFIG | 0x0C | Configuration update |
| CONFIG_ACK | 0x0D | Configuration acknowledgment |
| SESSION_START | 0x0E | Exercise session start |
| SESSION_END | 0x0F | Exercise session end |
| IMU_STREAM | 0x10 | IMU data stream (BLE) |
| FORCE_STREAM | 0x11 | Force data stream (BLE) |
| PRESSURE_FRAME | 0x12 | Pressure mat frame (Sub-GHz) |
| FORM_UPDATE | 0x13 | Form score update (Hub → cloud) |
| REP_COUNT | 0x14 | Rep count update |
| EXERCISE_ID | 0x15 | Exercise identification result |

## 3. Telemetry Subtypes

| Subtype | Value | Source |
|---------|-------|--------|
| BODY_SENSOR | 0x01 | Body Sensor node |
| SMART_BAND | 0x02 | Smart Band node |
| PRESSURE_MAT | 0x03 | Pressure Mat node |
| HUB | 0x04 | Hub node |

## 4. Alert Types

| Alert | Value | Severity | Description |
|-------|-------|----------|-------------|
| FORM_DEVIATION | 0x01 | Medium | Exercise form deviation detected |
| POOR_FORM | 0x02 | High | Form score below threshold |
| OVEREXERTION | 0x03 | High | Force exceeding safe range |
| FATIGUE_DETECTED | 0x04 | Medium | Form degradation over session |
| REGRESSION | 0x05 | High | ROM or performance regression |
| SENSOR_OFFLINE | 0x06 | Medium | Sensor disconnected |
| SENSOR_LOW_BATT | 0x07 | Low | Sensor battery < 20% |
| FALL_DETECTED | 0x08 | Critical | Fall detected during exercise |
| ROM_REGRESSION | 0x09 | High | Range of motion decreasing |
| ADHERENCE_DROP | 0x0A | Medium | Adherence rate declining |
| PAIN_INDICATOR | 0x0B | High | Pain-related movement pattern |
| SESSION_TIMEOUT | 0x0C | Low | Session idle timeout |

## 5. Command Types

| Command | Value | Description |
|---------|-------|-------------|
| START_SESSION | 0x01 | Start exercise session |
| STOP_SESSION | 0x02 | Stop exercise session |
| START_EXERCISE | 0x03 | Start specific exercise |
| STOP_EXERCISE | 0x04 | Stop current exercise |
| SET_EXERCISE | 0x05 | Set current exercise type |
| SET_TARGET_REPS | 0x06 | Set target rep count |
| SET_RESISTANCE | 0x07 | Set resistance target (band) |
| CALIBRATE | 0x08 | Trigger sensor calibration |
| REBOOT | 0x09 | Reboot node |
| SET_CONFIG | 0x0A | Update configuration |
| HAPTIC_FEEDBACK | 0x0B | Trigger haptic pattern |
| AUDIO_FEEDBACK | 0x0C | Trigger audio message |
| EMERGENCY_STOP | 0x0D | Emergency stop all exercises |
| PAUSE | 0x0E | Pause session |
| RESUME | 0x0F | Resume session |

## 6. IMU Data Packet (BLE GATT Notification)

```
┌───────────────────────────────────────────┐
│ IMU Sample (12 bytes)                     │
├───────┬───────┬───────┬───────┬───────┬───────┐
│ AccX  │ AccY  │ AccZ  │ GyrX  │ GyrY  │ GyrZ  │
│ int16 │ int16 │ int16 │ int16 │ int16 │ int16 │
│ mg    │ mg    │ mg    │ mdps  │ mdps  │ mdps  │
└───────┴───────┴───────┴───────┴───────┴───────┘
┌───────────────────────────────────────────┐
│ Quaternion (8 bytes, appended)            │
├───────┬───────┬───────┬───────┐
│  Q0   │  Q1   │  Q2   │  Q3   │
│ int16 │ int16 │ int16 │ int16 │
│ ±1    │ ±1    │ ±1    │ ±1    │
└───────┴───────┴───────┴───────┘
```

Total BLE notification: 20 bytes per sample at 100 Hz = 2000 bytes/s

## 7. Force Data Packet (BLE GATT Notification)

```
┌──────────────────────────────────────────────────────────┐
│ Force Sample (16 bytes)                                  │
├──────────┬───────────────────────┬───────────────────────┤
│ Force    │ Accel (3 × int16)     │ Gyro (3 × int16)      │
│ int32    │ 6 bytes               │ 6 bytes               │
│ mg-force │ mg                    │ mdps                  │
└──────────┴───────────────────────┴───────────────────────┘
```

Total BLE notification: 16 bytes per sample at 50 Hz = 800 bytes/s

## 8. Pressure Frame (Sub-GHz)

```
┌──────────────────────────────────────────────────────────┐
│ Pressure Frame Header (10 bytes)                         │
├───────────┬───────────┬───────────┬───────────┬──────────┤
│ FrameSeq  │ COP_X     │ COP_Y     │ TotalWt   │ Asymmetry│
│ uint16    │ uint16    │ uint16    │ uint16    │ uint16   │
│           │ 0-65535   │ 0-65535   │ grams     │ 0-1000   │
│           │ →0-15.99  │ →0-15.99  │           │          │
└───────────┴───────────┴───────────┴───────────┴──────────┘
┌──────────────────────────────────────────────────────────┐
│ Compressed Cells (128 bytes)                             │
│ 8×8 averaged grid, each cell uint16 pressure value       │
└──────────────────────────────────────────────────────────┘
```

Total frame: 138 bytes, transmitted at 30 Hz via Sub-GHz