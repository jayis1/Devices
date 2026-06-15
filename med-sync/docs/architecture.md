# MedSync - Architecture Overview

## System Architecture

MedSync is a 4-node BLE mesh system for medication adherence and health monitoring.

### Nodes

| Node | SoC | Role | Power | Communication |
|------|-----|------|-------|---------------|
| Hub | nRF52840 + ESP32-S3 | Coordinator, display, audio, WiFi bridge | USB-C + 18650 backup | BLE mesh coordinator, WiFi6, BLE GATT |
| Pill Station | STM32F407 + nRF52832 | Motorized dispenser, weight verification | USB-C 5V 3A + 18650 backup | BLE mesh router |
| Room Beacon | nRF52832 | Occupancy, environment, proximity reminders | CR2477 coin cell | BLE mesh router |
| Wearable Tag | nRF52833 | Pulse ox, fall detection, activity, haptic | CR2032 coin cell | BLE mesh low-power node |

### Communication Flow

```
Mobile App ←→ Cloud (MQTT) ←→ Hub ←→ BLE Mesh ←→ {Pill Station, Room Beacons, Wearable Tag}
                                       ↕
                                    NFC tap-to-confirm
```

### Data Flow

1. **Schedule**: Cloud → Hub (MQTT) → Pill Station (BLE mesh)
2. **Dose Trigger**: Hub → Pill Station (BLE mesh DoseTrigger)
3. **Dose Verification**: Pill Station → Hub (BLE mesh DoseConfirm)
4. **Vitals**: Wearable → Hub (BLE mesh) → Cloud (MQTT)
5. **Fall Alert**: Wearable → Hub (BLE mesh) → Cloud (MQTT) → Caregiver (Push/SMS)
6. **Proximity Reminder**: Hub → Room Beacon (BLE mesh) → LED + Buzzer when PIR detects patient

### Security

- BLE mesh uses AES-128-CCM encryption with separate network and application keys
- Pill station commands require 4-byte auth token (prevents accidental BLE spoofing)
- NFC tap-to-confirm is proximity-verified (physically present)
- Cloud API uses JWT authentication with 2FA for dose confirmation
- All MQTT traffic uses TLS (QoS 1 for critical events)

### Power Management

- **Hub**: 120mA average (WiFi on), 17h on battery backup
- **Pill Station**: 80mA idle, 800mA during motor operation, 24h on battery backup
- **Room Beacon**: 15µA average, 7.5+ years on CR2477
- **Wearable Tag**: 30µA average, ~9 months on CR2032

### Failure Modes

| Failure | Behavior |
|---------|----------|
| WiFi outage | Hub maintains local schedule, buffers data on SD card (7 days) |
| Cloud outage | All local reminders still work, data queued for sync |
| BLE mesh failure | Room beacons flash LED pattern as standalone reminders |
| Pill station motor fault | Alert hub + cloud, user can manually open cover |
| Wearable battery death | Hub continues schedule without vitals monitoring |
| Hub power loss | Pill station runs schedule independently from RTC |
| Weight sensor drift | Periodic calibration reminder, IR backup verification |