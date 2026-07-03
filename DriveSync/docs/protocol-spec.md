# DriveSync — Protocol Specification

## Packet Format

All DriveSync communications use a binary packet format over BLE 5.0:

```
┌──────────────────────────────────────────────────────────┐
│  Byte  │  Field         │  Description                     │
├──────────────────────────────────────────────────────────┤
│  0     │  sync[0]       │  0x44 ('D')                     │
│  1     │  sync[1]       │  0x53 ('S')                     │
│  2     │  version       │  0x01                           │
│  3     │  msg_type      │  See message type enum          │
│  4-5   │  sender_id     │  Node ID (little-endian)        │
│  6-7   │  seq_num       │  Sequence number (LE)           │
│  8     │  flags         │  Bit 0: encrypted               │
│        │                │  Bit 1: compressed               │
│        │                │  Bit 2: ACK requested           │
│  9     │  payload_len   │  0-245                           │
│  10    │  checksum      │  XOR of bytes 0-9               │
│  11+   │  payload       │  0-245 bytes                     │
└──────────────────────────────────────────────────────────┘
```

**Total:** 11 + payload_len bytes (max 256)

## Node IDs

| ID Range | Node |
|---------|------|
| 0x0000 | Hub (broadcast) |
| 0x0100-0x01FF | Steering Wheel Nodes |
| 0x0200-0x02FF | Seat Belt Tags |
| 0x0300-0x03FF | OBD-II Dongles |

## Message Types

| Type | Code | Direction | Payload |
|------|------|-----------|---------|
| DATA_CAMERA | 0x01 | Hub internal | payload_camera_t (20 bytes) |
| DATA_STEERING | 0x02 | Wheel → Hub | payload_steering_t (24 bytes) |
| DATA_PPG | 0x03 | Belt → Hub | payload_ppg_t (8 bytes) |
| DATA_OBD | 0x04 | OBD → Hub | payload_obd_t (13 bytes) |
| DATA_BODY_IMU | 0x05 | Belt → Hub | payload_body_imu_t (16 bytes) |
| DATA_HUB_IMU | 0x06 | Hub internal | payload_hub_imu_t (14 bytes) |
| CMD_MODE | 0x10 | Hub → nodes | payload_mode_t (1 byte) |
| CMD_PAIR | 0x11 | Hub → node | payload_pair_t (3 bytes) |
| ALERT_DROWSY | 0x20 | Hub → wheel/belt | payload_alert_t (4 bytes) |
| ALERT_DISTRACT | 0x21 | Hub → wheel | payload_alert_t (4 bytes) |
| ALERT_CRITICAL | 0x22 | Hub → belt + cloud | payload_alert_t (4 bytes) |
| ACK | 0x30 | Node → Hub | none |
| NACK | 0x31 | Node → Hub | error code (1 byte) |
| HEARTBEAT | 0x50 | Hub → nodes | none |
| STATUS | 0x51 | Node → Hub | payload_status_t (3 bytes) |

## Payload Structures

### payload_camera_t (20 bytes)
```c
float    perclos;         // 0.0-1.0
uint16_t blink_rate;      // blinks/min
uint16_t avg_blink_dur;   // ms
int16_t  head_pitch;      // centi-degrees
int16_t  head_yaw;
int16_t  head_roll;
uint8_t  head_bob_count;
uint8_t  confidence;      // 0-100
uint32_t timestamp;       // ms since boot
```

### payload_steering_t (24 bytes)
```c
int16_t  gyro_z;          // milli-degrees/sec
int16_t  accel_x;         // milli-g
int16_t  accel_y;
int16_t  accel_z;
uint16_t jerk_count;      // reversal count (100ms)
uint16_t grip_raw[4];     // FDC2214 readings
uint8_t  hands_on;        // 0/1
uint8_t  grip_force;      // 0-100
uint32_t timestamp;
```

### payload_ppg_t (8 bytes)
```c
uint8_t  hr;              // bpm
uint8_t  hrv_rmssd;      // ms
uint8_t  pnn50;          // %
uint8_t  spo2;           // %
uint8_t  confidence;
uint32_t timestamp;
```

### payload_obd_t (13 bytes)
```c
uint16_t speed_kmh;
uint16_t rpm;
uint8_t  throttle_pct;
uint8_t  engine_load;
int16_t  coolant_temp_c;  // centi-degrees
uint8_t  fuel_level;
uint8_t  obd_pid_flags;
uint32_t timestamp;
```

### payload_alert_t (4 bytes)
```c
uint8_t  risk_score;      // 0-100
uint8_t  alert_level;     // 0-4
uint8_t  source;          // 0=perclos, 1=steering, 2=hrv, 3=fusion
uint8_t  duration_sec;
```

## Checksum

XOR checksum over bytes 0-9 (header excluding checksum field).

```c
uint8_t checksum = 0;
for (int i = 0; i < 10; i++) checksum ^= buf[i];
```

## Encryption

Optional AES-128-CTR encryption (flag bit 0 set). Session key derived via ECDH P-256 at pairing time. Nonce is 12 bytes, counter is 4 bytes (big-endian).