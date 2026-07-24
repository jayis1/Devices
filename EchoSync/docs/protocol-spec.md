# EchoSync — Communication Protocol Specification

## 1. Physical Layer

| Parameter | Value |
|-----------|-------|
| Band (EU) | 868 MHz ISM |
| Band (US) | 915 MHz ISM |
| Modulation | LoRa (SX1262) |
| Bandwidth | 125 kHz |
| Spreading Factor | 7 (adaptive 7–11) |
| Coding Rate | 4/5 |
| TX Power | +22 dBm |
| Preamble | 8 symbols |
| Range (SF7) | 300 m LOS |
| Range (SF11) | 2 km + mesh |
| BLE | 5.0 (2.4 GHz) |
| BLE Range | 15 m LOS |
| Encryption | AES-128-CCM |

## 2. MAC Layer

### Sub-GHz: TDMA Mesh
- **Frame duration:** 1000 ms (20 slots × 50 ms)
- **Slot assignment:** Hub assigns on JOIN_ACK
- **Coordinator:** Hub (node_id=0, slot=0)
- **Relay:** Nodes can relay for out-of-range peers
- **Max nodes:** 16

### BLE 5.0: GATT
- **Connection:** Wrist Band and Door Tag connect to Hub
- **Service UUID:** 0000E550-0000-1000-8000-00805F9B34FB
- **Sound Event Char:** 0000E551 (Write: Hub→Wrist Band)
- **Door Event Char:** 0000E561 (Notify: Door Tag→Hub)

## 3. Message Format

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x45 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

- **Sync:** `0x45 0x53` ("ES" = EchoSync)
- **CRC:** CRC-16-CCITT (polynomial 0x1021)
- **Max payload:** 240 bytes
- **Max message:** 256 bytes

## 4. Message Types

| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x01 | JOIN_REQ | Node→Hub | Join network request |
| 0x02 | JOIN_ACK | Hub→Node | Node ID + slot assignment |
| 0x03 | TELEMETRY | Node→Hub | Sensor data / sound event |
| 0x04 | COMMAND | Hub→Node | Actuator command |
| 0x05 | CMD_ACK | Node→Hub | Command acknowledgment |
| 0x06 | ALERT | Node→Hub | Alert (emergency, low battery) |
| 0x07 | OTA_BLOCK | Hub→Node | Firmware update chunk |
| 0x08 | OTA_ACK | Node→Hub | Firmware chunk ack |
| 0x09 | HEARTBEAT | Node→Hub | Alive + battery + RSSI |
| 0x0A | MESH_RELAY | Node→Node | Forwarded message |
| 0x0B | SOUND_EVENT | Hub→Wrist | Sound event for haptic alert |
| 0x0C | TIME_SYNC | Hub→Node | Epoch time (TDMA alignment) |
| 0x0D | CONFIG | Hub→Node | Sampling configuration |
| 0x0E | CONFIG_ACK | Node→Hub | Config confirmed |
| 0x0F | SOUND_ENROLL | Hub→Sentinel | Start custom sound enrollment |
| 0x10 | ENROLL_SAMPLE | Sentinel→Hub | Custom sound sample data |
| 0x11 | DISPLAY_UPDATE | Hub→Hub | E-ink display update |

## 5. Telemetry Payloads

### Room Sentinel (Sub-type 0x01, 18 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V (0xFF=USB) |
| 2 | Sound class | 1 | 0–19 |
| 3 | Confidence | 1 | ×1% |
| 4 | Direction (azimuth) | 2 | ×0.1 degrees |
| 6 | Direction (elevation) | 1 | ×1 degree (signed) |
| 7 | Duration | 2 | ×1 ms |
| 9 | Temp | 2 | ×0.1°C |
| 11 | Humidity | 2 | ×0.1% RH |
| 13 | dB SPL | 1 | ×1 dB SPL |
| 14 | Priority | 1 | 0/1/2 |
| 15 | Event ID | 2 | counter |
| 17 | RSSI | 1 | signed dBm |

### Wrist Band (Sub-type 0x02, 10 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Worn status | 1 | 0/1 |
| 3 | Sleep status | 1 | 0/1 |
| 4 | Last alert class | 1 | sound class or 0xFF |
| 5 | Last alert priority | 1 | 0/1/2 |
| 6 | Alerts 24h | 2 | count |
| 8 | RSSI | 1 | signed dBm (0xFF=BLE) |

### Door Tag (Sub-type 0x03, 10 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Event type | 1 | 0=knock, 1=doorbell, 2=phone, 3=custom |
| 3 | Confidence | 1 | ×1% |
| 4 | Knock count | 1 | number of knocks |
| 5 | Event ID | 2 | counter |
| 7 | RSSI | 1 | signed dBm (0xFF=BLE) |

## 6. Sound Event Broadcast (Hub→Wrist Band, 12 bytes)

| Offset | Field | Size | Value |
|--------|-------|------|-------|
| 0 | Sound class | 1 | 0–19 |
| 1 | Priority | 1 | 0/1/2 |
| 2 | Confidence | 1 | 0–100% |
| 3 | Direction | 2 | ×0.1 degrees |
| 5 | Source node | 1 | sentinel node ID |
| 6 | Room hash | 2 | hash of room name |
| 8 | Event ID | 2 | counter |
| 10 | Haptic pattern | 1 | DRV2605L waveform ID |
| 11 | Reserved | 1 | 0x00 |

## 7. Sound Classes (20)

| Class | Name | Priority | Haptic |
|-------|------|----------|--------|
| 0 | Smoke Alarm | Emergency | Triple-burst |
| 1 | CO Alarm | Emergency | Triple-burst |
| 2 | Glass Break | Emergency | Triple-burst |
| 3 | Siren | Emergency | Triple-burst |
| 4 | Doorbell | Important | Double-pulse |
| 5 | Door Knock | Important | Double-pulse |
| 6 | Phone Ring | Important | Double-pulse |
| 7 | Baby Crying | Important | Double-pulse |
| 8 | Car Horn | Important | Double-pulse |
| 9 | Door Open | Info | Single-tap |
| 10 | Door Close | Info | Single-tap |
| 11 | Running Water | Info | Single-tap |
| 12 | Dog Bark | Info | Single-tap |
| 13 | Alarm Clock | Info | Single-tap |
| 14 | Microwave Beep | Info | Single-tap |
| 15 | Dishwasher | Info | Single-tap |
| 16 | Washing Machine | Info | Single-tap |
| 17 | Person Entering | Info | Single-tap |
| 18 | Custom Sound 1 | User-set | User-configured |
| 19 | Custom Sound 2 | User-set | User-configured |

## 8. Haptic Patterns (DRV2605L)

| Priority | Pattern | DRV2605L Effect | Duration |
|----------|---------|-----------------|----------|
| Emergency | Triple-burst | Effect 73 ×3 (150ms gap) | 500 ms |
| Important | Double-pulse | Effect 47 | 300 ms |
| Info | Single-tap | Effect 12 | 100 ms |

## 9. Join Process

1. New node powers on → sends `JOIN_REQ` (dst=0x00, src=0xFF)
2. Hub assigns node ID + TDMA slot → sends `JOIN_ACK`
3. Node stores assignment → begins operating in assigned slot
4. Hub broadcasts `TIME_SYNC` every 60 seconds

## 10. Alert Types

| Type | Alert | Severity |
|------|-------|----------|
| 0x01 | Low battery | Warning |
| 0x02 | Sound event | Info/Important/Emergency |
| 0x03 | Emergency sound | Critical |
| 0x04 | Node offline | Warning |
| 0x05 | Sensor anomaly | Warning |
| 0x06 | Enrollment complete | Info |
| 0x07 | Custom sound learned | Info |

## 11. Command Types

| Type | Command | Target |
|------|---------|--------|
| 0x01 | BED_SHAKER_ON | Hub |
| 0x02 | BED_SHAKER_OFF | Hub |
| 0x03 | BUZZER_ON | Hub |
| 0x04 | BUZZER_OFF | Hub |
| 0x05 | EMERGENCY_MODE | All nodes |
| 0x06 | NORMAL_MODE | All nodes |
| 0x07 | SET_CONFIG | Any node |
| 0x08 | REBOOT | Any node |
| 0x09 | CALIBRATE | Any node |
| 0x0A | START_ENROLLMENT | Room Sentinel |
| 0x0B | STOP_ENROLLMENT | Room Sentinel |
| 0x0C | HAPTIC_PATTERN | Wrist Band |
| 0x0D | DISPLAY_UPDATE | Hub E-ink |
| 0x0E | SILENCE_ALERTS | Wrist Band |