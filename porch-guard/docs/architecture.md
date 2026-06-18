# PorchGuard — Architecture & Protocol Docs

## System Overview

PorchGuard is a 4-node IoT system for intelligent porch security and delivery management:

| Node | MCU | Role | Power |
|------|-----|------|-------|
| Hub | RP2040 + ESP32-C6 | Mesh coordinator, ML inference, display, siren, cloud bridge | USB-C + LiPo backup |
| Porch Camera | ESP32-S3 (8MB PSRAM) | Package detection, person re-ID, pirate telemetry, clips, two-way audio | USB-C (doorbell) + supercap |
| Mailbox | STM32L011 | Mail arrival/classification, tamper detection, long-range uplink | CR2032 + solar |
| Lock | nRF52840 | Motorized deadbolt, keypad, garage relay, one-time courier codes | 4× AA (8-12 months) |

## Mesh Protocol Specification

### Physical Layer
- **Radio:** SX1261/62, 915MHz (US) / 868MHz (EU)
- **Modulation:** LoRa — SF7 (normal), SF9 (mailbox long-range), SF12 (pirate/tamper alarm)
- **Bandwidth:** 125kHz
- **TX Power:** +14dBm (nodes), +20dBm (hub)
- **Range:** 30m indoor (typical), 200m (SF9), 500m+ (SF12)
- **Sync Word:** 0x5047 ("PG")

### MAC Layer
- **Access:** TDMA (Time Division Multiple Access)
- **Hub is coordinator:** broadcasts sync beacon in Slot 0
- **Frame:** 5 slots × 100ms = 500ms
- **Slot 0:** Hub broadcast (sync + commands + armed state)
- **Slot 1:** Porch camera uplink (events/alerts/telemetry)
- **Slot 2:** Mailbox uplink (mail/tamper, long-range SF9 when polled)
- **Slot 3:** Lock uplink (door state/tamper, usually BLE, Sub-GHz on fallback)
- **Slot 4:** Control / ACK / retransmit / OTA

### Pirate/Tamper Alert Override
- When porch camera detects pirate risk >0.8, it immediately broadcasts
  a PIRATE_ALERT packet on SF12 (max range + robustness)
- All nodes halt normal TDMA, relay the alert
- Hub activates 100 dB siren + forwards clip ref to cloud/app
- Same override for lock forced-entry and mailbox tamper
- Normal TDMA resumes after 5 seconds of no alert packets

### Network Layer
- **Addressing:** 8-bit node IDs (0x00=hub, 0x01=camera, 0x02=mailbox, 0x03=lock)
- **Broadcast:** 0xFF destination
- **Timeout:** Node marked inactive after 120s without heartbeat

### Application Layer
- Packet types: CAMERA_DATA, MAILBOX_DATA, LOCK_DATA, COMMAND, ACK,
  OTA_BLOCK, CALIBRATION, PIRATE_ALERT, TAMPER_ALERT, DELIVERY_EVENT,
  HEARTBEAT, CLIP_REF
- See `firmware/common/mesh_protocol.h` for full struct definitions

## BLE Lock Channel (nRF52840)

| Parameter | Value |
|-----------|-------|
| Profile | Custom GATT + standard HID for keypad |
| Advertising | 100ms (when active) / 1s (sleep) |
| Connection | Encrypted (LE Secure Connections, AES-CCM) |
| Range | ~10m |
| Bonding | Phone + hub paired at setup |

## WiFi / BLE Bridge (ESP32-C6 on Hub)

### MQTT Topics
- `porchguard/camera_data` — Camera telemetry (JSON)
- `porchguard/mailbox_data` — Mailbox telemetry (JSON)
- `porchguard/lock_data` — Lock telemetry (JSON)
- `porchguard/pirate_alert` — Critical pirate alert (JSON)
- `porchguard/tamper_alert` — Tamper alert (JSON)
- `porchguard/delivery_event` — Delivery/mail event (JSON)
- `porchguard/clip_ref` — Clip reference (JSON)
- `porchguard/lock_event` — Lock/unlock event (JSON)
- `porchguard/alerts` — General system alerts (JSON)
- `porchguard/commands/unlock` — Unlock command
- `porchguard/commands/lock` — Lock command
- `porchguard/commands/garage` — Garage relay command
- `porchguard/commands/issue_code` — Issue courier code
- `porchguard/commands/revoke_code` — Revoke courier code
- `porchguard/commands/arm` — Arm/disarm
- `porchguard/commands/siren` — Trigger siren
- `porchguard/commands/siren_off` — Stop siren

## ML Pipeline

### On-Device (TFLite Micro)
1. **Package detector** (camera, ~210 KB) — MobileNet-SSD lite, 5 classes
2. **Person re-ID** (camera, ~90 KB) — 128-d embedding, cosine similarity gallery
3. **Pirate behavior** (hub, ~110 KB) — 1D-CNN + LSTM, 30s window @ 2Hz

### Cloud
4. **Anomaly detection** — Isolation Forest on 30-day event log (unusual-hours activity)
5. **Mail classification refinement** — learns typical mail weight distribution

## Power Architecture

| Node | Source | Backup | Lifetime |
|------|--------|--------|----------|
| Hub | USB-C 5V | LiPo 2500mAh | 12+ hrs on battery |
| Camera | USB-C (doorbell 16-24VAC → 5V buck) | 1F supercap (5s) | continuous (wired) |
| Mailbox | 2× CR2032 + 0.5W solar | — | months+ (solar trickle) |
| Lock | 4× AA | — | 8-12 months |

## Security Considerations

- Siren path (camera → Sub-GHz → hub → siren) works even if WiFi is down
- Camera supercap fires final TAMPER_ALERT if doorbell power is cut
- Hub battery backup keeps mesh + siren + cloud alive during outage
- Mailbox is fully battery/solar — unreachable from porch, always reports
- BLE lock uses LE Secure Connections (AES-CCM) encryption
- One-time courier codes are single-use, time-windowed, stored in flash
- Clip pre-buffer in PSRAM ensures the 5s before an event is always captured