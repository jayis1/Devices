# WashWise — Architecture & Protocol Docs

## System Overview

WashWise is a 4-node IoT system for intelligent laundry care and fire safety:

| Node | MCU | Role | Power |
|------|-----|------|-------|
| Hub | RP2040 + ESP32-C6 | Mesh coordinator, ML inference, display, alarm, cloud bridge | USB-C + LiPo backup |
| Washer | ESP32-S3 | Cycle monitoring, auto-dosing, leak detection | 12V + USB-C |
| Dryer | ESP32-S3 | **Fire safety** — lint clog, overheating, dryness detection | USB-C + LiPo backup |
| Scanner | ESP32-S3 | Multispectral garment scanning, fabric + stain ID | LiPo (USB-C charge) |

## Mesh Protocol Specification

### Physical Layer
- **Radio:** SX1261/62, 868MHz (EU) / 915MHz (US)
- **Modulation:** LoRa — SF7 (normal), SF10 (fire alerts / long range)
- **Bandwidth:** 125kHz
- **TX Power:** +14dBm (nodes), +20dBm (hub)
- **Range:** 30m indoor (typical), 200m (long range)
- **Sync Word:** 0x5A5A ("WW")

### MAC Layer
- **Access:** TDMA (Time Division Multiple Access)
- **Hub is coordinator:** broadcasts sync beacon in Slot 0
- **Frame:** 5 slots × 100ms = 500ms
- **Slot 0:** Hub broadcast (sync + commands)
- **Slot 1:** Washer node uplink
- **Slot 2:** Dryer node uplink (SAFETY PRIORITY)
- **Slot 3:** Scanner node uplink (when active)
- **Slot 4:** Control / ACK / retransmit / OTA

### Fire Alert Override
- When dryer node detects fire risk >0.8, it immediately broadcasts
  a FIRE_ALERT packet on SF10 (long range, robust modulation)
- All nodes halt normal TDMA, relay the alert
- Hub activates local piezo alarm (85 dB) + forwards to cloud/app
- Normal TDMA resumes after 5 seconds of no alert packets

### Network Layer
- **Addressing:** 8-bit node IDs (0x00=hub, 0x01=washer, 0x02=dryer, 0x03=scanner)
- **Broadcast:** 0xFF destination
- **Timeout:** Node marked inactive after 60s without heartbeat

### Application Layer
- Packet types: WASHER_DATA, DRYER_DATA, SCAN_RESULT, COMMAND, ACK,
  OTA_BLOCK, CALIBRATION, FIRE_ALERT, ENERGY_DATA, HEARTBEAT
- See `firmware/common/mesh_protocol.h` for full struct definitions

## WiFi / BLE Bridge (ESP32-C6 on Hub)

### MQTT Topics
- `washwise/washer_data` — Washer telemetry (JSON)
- `washwise/dryer_data` — Dryer telemetry (JSON, includes fire_risk_score)
- `washwise/scan_result` — Stain scanner results (JSON)
- `washwise/fire_alert` — Critical fire alert (JSON)
- `washwise/energy_data` — Per-cycle energy/water usage (JSON)
- `washwise/alerts` — General system alerts (JSON)
- `washwise/commands/dose` — Detergent dosing command
- `washwise/commands/cycle` — Cycle selection command
- `washwise/commands/dryer_shutoff` — Dryer shutoff advisory
- `washwise/ota/{node_id}` — OTA firmware blocks

### BLE GATT Service
- Service UUID: `0x5A01` (WashWise)
- Char `0x5A02`: System status (read/notify) — fire risk, cycle state
- Char `0x5A03`: Washer data (read/notify)
- Char `0x5A04`: Dryer data (read/notify) — includes fire_risk_score
- Char `0x5A05`: Command write (write) — dose, cycle select
- Char `0x5A06`: WiFi config (write)
- Char `0x5A07`: Device info (read)

## Alert Priority Levels

| Level | Notification | Local Action |
|-------|-------------|-------------|
| INFO | Dashboard only | LED blue blink |
| WARNING | Push notification | LED orange + 2 beeps |
| CRITICAL | Push + SMS | LED red + 4 beeps + "clean lint trap NOW" |
| EMERGENCY | Push + SMS + email | Continuous 85 dB piezo alarm + dryer shutoff advisory |

## Fire Safety Logic

### Lint Fire Risk Scoring

The fire risk score (0.0-1.0) is computed from 6 sensor inputs:

1. **Exhaust temperature** (primary indicator):
   - >105°C → +0.9 (heating element malfunction)
   - >95°C → +0.7 (severe overheating)
   - >85°C → +0.4 (warning)
   - >75°C → +0.15 (elevated)

2. **Differential pressure** (lint clog amplifier):
   - >200 Pa → +0.3 (severe clog)
   - >120 Pa → +0.15 (moderate clog)

3. **Low humidity + heating** (dry lint = more flammable):
   - Heating on + humidity <20% → +0.15

4. **Smoke detection** (MQ-2):
   - >200 ppm → score = 0.95 (immediate emergency)

### Trigger Thresholds
- >0.6 = WARNING ("clean lint trap soon")
- >0.8 = CRITICAL ("clean lint trap NOW" + push notification)
- >0.95 = EMERGENCY (continuous alarm + SMS + dryer shutoff advisory)

### Lint Clog Levels (differential pressure)
| Level | Pressure (Pa) | Meaning |
|-------|---------------|---------|
| Clean | 10-80 | Normal airflow |
| Mild | 80-120 | Beginning to clog |
| Moderate | 120-200 | Needs cleaning soon |
| Severe | >200 | Fire risk — clean immediately |

## Dosing Engine Logic

```
1. Receive scan result from scanner (fabric + stain + recommended dose)
   OR user selects cycle via app
2. Apply corrections:
   a. Load weight (estimated from vibration/load cell)
   b. Water hardness (from local water database or user setting)
   c. Detergent brand concentration (learned over time)
3. Send CMD_DOSE to washer node
4. Washer node dispenses via peristaltic pump during fill phase
5. Verify dispensed amount via load cell weight change
6. Log dose to cloud
7. Monitor wash effectiveness (vibration signature during wash)
8. Adjust future doses based on outcome (reinforcement learning)
```

## Dryness Detection

Exhaust humidity is the key indicator:

| Humidity | Dryness | Action |
|----------|---------|--------|
| >55% | Wet | Keep drying |
| 40-55% | Damp | Almost done |
| 25-40% | Dry | ✅ Stop (optimal) |
| <25% | Over-dry | ⚠️ Stop — wasting energy + damaging fabric |

Stopping at "Dry" instead of a fixed timer saves 15-30% energy and
prevents fabric damage from over-drying.

## OTA Update Protocol

1. Dashboard uploads new firmware binary to cloud
2. Cloud sends OTA_BLOCK packets via MQTT → hub
3. Hub caches blocks, verifies CRC per block
4. Hub broadcasts OTA_BLOCK to target node in slot 4
5. Target node writes to flash, verifies, sends ACK
6. On all blocks received + verified, target reboots into new firmware
7. Hub monitors heartbeat from updated node
8. If no heartbeat in 60s, rollback to previous firmware

## Power Architecture

| Node | Primary | Backup | Runtime on Backup |
|------|----------|--------|-------------------|
| Hub | USB-C 5V | LiPo 2000mAh | 8+ hours |
| Washer | 12V (pump) + USB-C (MCU) | — | — |
| Dryer | USB-C 5V | LiPo 500mAh | 2+ hours |
| Scanner | LiPo 1000mAh | USB-C charging | 1 week standby |

**Critical:** The dryer node has battery backup. If power goes out while
the dryer was running (thermal mass still hot), the node keeps monitoring
for fire risk. The hub also has backup to relay alerts.

## Data Flow

```
Scanner → (scan garment) → Hub → Cloud ML (stain ID) → App (recommendation)
                                        ↓
User loads washer → Washer Node → Hub → Cloud (dose optimization)
                     ↑ (auto-dose)
                     Hub sends CMD_DOSE

Dryer running → Dryer Node → Hub (fire risk ML) → App + Cloud
                   ↓ (fire alert if risk >0.8)
                   Hub → piezo alarm + SMS + app push
```