# ErgoFlow — BLE Mesh Protocol Specification

## Overview

ErgoFlow uses Bluetooth Mesh (v1.0.1) for all inter-node communication. The hub node acts as the Provisioner and relay. All other nodes communicate exclusively through the hub in a star topology.

## Network Configuration

| Parameter | Value |
|-----------|-------|
| Mesh Version | 1.0.1 |
| Bearer | ADV (advertising channels 37, 38, 39) |
| Security | AES-CCM 128-bit |
| Provisioning | OOB Numeric (4-digit) |
| Network Key | 1 key (index 0x0000) |
| Application Keys | 2 (control: 0x0000, telemetry: 0x0001) |
| TTL | 7 hops |
| Retransmit | 3 retries, 100ms interval |
| Relay | Hub only |

## Node Addresses

| Node | Unicast Address | Elements |
|------|-----------------|----------|
| Hub | 0x0001 | 1 |
| Chair Pad | 0x0002 | 1 |
| Desk Controller | 0x0003 | 1 |
| Wearable Tag 1 | 0x0004 | 1 |
| Wearable Tag 2 | 0x0005 | 1 |

## Message Format

All messages use the ErgoFlow vendor model with the following format:

```
┌─────────┬─────────┬──────────────┬─────────────┐
│ Opcode  │  Msg    │   Payload    │   Padding   │
│ (2B)    │  Len    │   (0-NB)     │   (0-3B)    │
└─────────┴─────────┴──────────────┴─────────────┘
```

## Message Types

### 0xC001 — PRESSURE_MAP
**Direction:** Chair Pad → Hub  
**Interval:** 500ms (2Hz)  
**Payload:** 17 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 8 | seat_pressure | Seat FSR values S1-S8 (0-255) |
| 8 | 8 | back_pressure | Backrest FSR values B1-B8 (0-255) |
| 16 | 1 | imu_flags | Bit flags: sitting, moving, tilt |

### 0xC002 — IMU_ORIENTATION
**Direction:** Wearable Tag → Hub  
**Interval:** 500ms (2Hz)  
**Payload:** 18 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 16 | quaternion | 4× float32 (w, x, y, z) |
| 16 | 1 | activity | Activity class (0-5) |
| 17 | 1 | confidence | Confidence 0-100% |

### 0xC003 — HEART_RATE
**Direction:** Wearable Tag → Hub  
**Interval:** 60000ms (1/min)  
**Payload:** 2 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | hr_bpm | Heart rate in BPM |
| 1 | 1 | spo2_pct | SpO2 percentage |

### 0xC004 — DESK_COMMAND
**Direction:** Hub → Desk Controller  
**Event-driven**  
**Payload:** 4 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | cmd | 0x01=height, 0x02=preset, 0x03=stop |
| 1 | 2 | target_mm | Target height in mm (little-endian) |
| 3 | 1 | speed_pct | Speed 0-100% |

### 0xC005 — DESK_STATUS
**Direction:** Desk Controller → Hub  
**Interval:** 2000ms (0.5Hz)  
**Payload:** 5 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 2 | height_mm | Current height in mm (little-endian) |
| 2 | 1 | motor_state | 0=idle, 1=up, 2=down, 3=error |
| 3 | 2 | current_ma | Motor current in mA (little-endian) |

### 0xC006 — AMBIENT_READING
**Direction:** Hub → Cloud (via ESP32-C6)  
**Interval:** 5000ms (0.2Hz)  
**Payload:** 10 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 4 | lux | Ambient light in lux (uint32, LE) |
| 4 | 2 | temp_celsius | Temperature in 0.1°C units (int16, LE) |
| 6 | 2 | humidity_pct | Humidity in 0.1% units (uint16, LE) |
| 8 | 2 | reserved | Future use |

### 0xC007 — POSTURE_SCORE
**Direction:** Hub → All  
**Interval:** 500ms (2Hz)  
**Payload:** 4 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | score | Posture quality 0-100 |
| 1 | 1 | risk_level | 0=low, 1=medium, 2=high |
| 2 | 2 | duration_s | Seconds in current posture (uint16, LE) |

### 0xC008 — BREAK_REMINDER
**Direction:** Hub → All  
**Event-driven**  
**Payload:** 3 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | type | 0=stretch, 1=walk, 2=look_away |
| 1 | 2 | duration_s | Suggested break duration in seconds (uint16, LE) |

### 0xC009 — LIGHTING_CMD
**Direction:** Hub → Desk Controller  
**Event-driven / 5min periodic**  
**Payload:** 6 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | r | Red channel 0-255 |
| 1 | 1 | g | Green channel 0-255 |
| 2 | 1 | b | Blue channel 0-255 |
| 3 | 1 | w | White channel 0-255 |
| 4 | 1 | brightness_pct | Brightness 0-100% |
| 5 | 1 | mode | 0=manual, 1=circadian, 2=focus, 3=relax |

### 0xC00A — MONITOR_TILT
**Direction:** Hub → Desk Controller  
**Event-driven**  
**Payload:** 2 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | tilt_degrees | -15 to +15 degrees (int8) |
| 1 | 1 | speed_pct | Speed 0-100% |

### 0xC00B — OTA_AVAILABLE
**Direction:** Hub → All  
**Event-driven**  
**Payload:** 20 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 4 | firmware_size | Total firmware size in bytes (uint32, LE) |
| 4 | 16 | sha256_hash | First 16 bytes of SHA256 hash |

### 0xC00C — OTA_DATA
**Direction:** Hub → Target Node  
**Continuous**  
**Payload:** 18 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 2 | seq_num | Sequence number (uint16, LE) |
| 2 | 16 | chunk | Firmware data chunk |

### 0xC00D — NODE_HEARTBEAT
**Direction:** All → Hub  
**Interval:** 60000ms (1/min)  
**Payload:** 4 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | battery_pct | Battery percentage 0-100 |
| 1 | 1 | state | Node state (0-5) |
| 2 | 2 | uptime_min | Uptime in minutes (uint16, LE) |

### 0xC00E — CALIBRATION
**Direction:** Hub → Any  
**Event-driven**  
**Payload:** 9 bytes

| Offset | Size | Name | Description |
|--------|------|------|-------------|
| 0 | 1 | target | 0=pressure, 1=imu, 2=desk |
| 1 | 4 | param1 | First parameter (uint32, LE) |
| 5 | 4 | param2 | Second parameter (uint32, LE) |

### 0xC00F — FACTORY_RESET
**Direction:** Hub → Any  
**Event-driven**  
**Payload:** 0 bytes

No payload. Resets node to factory defaults.

## Hub ↔ ESP32-C6 UART Protocol

The hub communicates with the ESP32-C6 WiFi co-processor via UART (115200 baud, 8N1).

### Frame Format

```
┌────────┬────────┬────────┬────────┬──────────┬──────────┐
│ Sync   │ Length │ Opcode │ Payload │  Reserved│ CRC16   │
│ 2B     │ 2B     │ 1B     │ NB     │  (opt)   │ 2B      │
└────────┴────────┴────────┴────────┴──────────┴──────────┘
```

- **Sync**: 0xAA55 (little-endian)
- **Length**: Payload length (little-endian)
- **Opcode**: See table below
- **CRC16**: CCITT CRC over all preceding bytes

### Opcodes

| Opcode | Name | Direction | Description |
|--------|------|-----------|-------------|
| 0x01 | MQTT_PUBLISH | Hub→ESP32 | Publish to MQTT topic |
| 0x02 | MQTT_SUBSCRIBE | Hub→ESP32 | Subscribe to MQTT topic |
| 0x03 | WIFI_SCAN | Hub→ESP32 | Scan WiFi networks |
| 0x04 | WIFI_CONNECT | Hub→ESP32 | Connect to WiFi |
| 0x05 | OTA_DATA | Hub→ESP32 | Firmware data |
| 0x06 | HTTP_GET | Hub→ESP32 | HTTP GET request |
| 0x81 | MQTT_MESSAGE | ESP32→Hub | Incoming MQTT message |
| 0x82 | WIFI_STATUS | ESP32→Hub | WiFi connection status |
| 0x83 | OTA_PROGRESS | ESP32→Hub | OTA download progress |
| 0x84 | HTTP_RESPONSE | ESP32→Hub | HTTP response data |