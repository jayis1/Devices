# WashWise

**AI-powered multi-node laundry care, fire-safety & sustainability system.** Keeps your clothes pristine, your dryer safe, and your laundry room smart — autonomously.

---

## What It Does

WashWise is a 4-node system that turns any laundry setup into an intelligent, self-regulating, fire-safe system:

1. **Scans** garments before washing — identifies fabric type, detects stains, reads care labels, and recommends the optimal treatment + wash cycle
2. **Doses** detergent automatically — per-load, based on fabric type, soil level, load weight, and water hardness (no more overdosing that damages clothes and machines)
3. **Monitors** your washer non-invasively — vibration analysis detects load imbalance, fabric type, and cycle completion; flow sensors track water usage
4. **Guards** your dryer against lint fires — differential pressure detects lint-trap clogs, exhaust thermocouple catches overheating, humidity tracking determines dryness so you never over-dry (saves energy + prevents fabric damage)
5. **Alerts** you before disasters — lint fire risk prediction, washer leak detection, cycle completion, detergent-low
6. **Learns** your laundry habits over time — transfer learning on your machine's vibration signatures, your local water response, your detergent brand

All nodes communicate over a dedicated Sub-GHz LoRa mesh network (no WiFi dependency for safety-critical functions). The hub bridges to WiFi/cloud for the dashboard and mobile app.

### The Problem It Solves

- **Lint fires:** Clothes dryers cause ~2,900 home fires/year in the US alone — 5 deaths, 100 injuries, $35M in property damage. **34% are caused by failure to clean.** WashWise's dryer node detects lint buildup and overheating *before* ignition.
- **Detergent waste & damage:** The average household uses 2-10× the necessary detergent. Overdosing causes skin irritation, leaves residue, damages machine seals, and wastes money. WashWise doses precisely per load.
- **Fabric damage:** Wrong cycles ruin clothes. Americans discard 11M tons of textiles/year, much of it prematurely damaged. WashWise scans fabric type and recommends the right cycle + temperature.
- **Stain guesswork:** Most people either ignore stains (set permanently) or use the wrong treatment (bleach on protein stains, hot water on blood). WashWise's multispectral scanner identifies stain chemistry.
- **Energy waste:** Overdrying wastes energy and damages fibers. WashWise's humidity-based dryness detection stops the dryer exactly when clothes are done — saving 15-30% on drying energy.
- **Forgotten laundry:** Wet clothes left in the washer mildew in 8-12 hours. WashWise reminds you and can interface with smart washers.

WashWise automates and de-risks all of this. You scan, load, and let the system handle the rest — with active fire safety running 24/7.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         WASHWISE SYSTEM                                   │
│                                                                           │
│  ┌───────────────┐   Sub-GHz    ┌───────────────┐                        │
│  │ STAIN SCANNER │◄───────────►│               │                        │
│  │ (handheld)    │   868MHz    │               │                        │
│  │ Camera+UV+IR  │   LoRa mesh │               │                        │
│  │ Fabric ID     │            │   HUB NODE    │                        │
│  │ Stain ID      │            │  (RP2040 +    │──── WiFi6 ───► Cloud    │
│  └───────────────┘            │   ESP32-C6)   │                  Dashboard
│                               │               │                  + ML    │
│  ┌───────────────┐            │               │                  Pipeline
│  │  WASHER NODE  │◄──────────►│               │                  + Alerts│
│  │ (on washer)   │   Sub-GHz  │               │                        │
│  │ Vibration     │   mesh     │               │─── BLE ──────► Mobile   │
│  │ Flow/Temp     │            │               │                 (React  │
│  │ Auto-dose     │            │               │                  Native)│
│  │ pump+load cell│            └───────┬───────┘                        │
│  └───────────────┘                    │ Sub-GHz mesh                    │
│  ┌───────────────┐                    │                                 │
│  │  DRYER NODE    │◄───────────────────┘                                │
│  │ (on dryer)     │   (SAFETY-CRITICAL:                                │
│  │ Lint pressure  │    fire risk 24/7)                                 │
│  │ Exhaust temp   │                                                    │
│  │ Humidity       │                                                    │
│  │ Vibration      │                                                    │
│  └───────────────┘                                                     │
│                                                                           │
│  ┌───────────────────────────────────────────────────────────────────┐   │
│  │                    CLOUD / EDGE SOFTWARE                          │   │
│  │  ┌─────────┐  ┌──────────────┐  ┌───────────────────────┐        │   │
│  │  │Dashboard│  │ ML Pipeline  │  │ Mobile App            │        │   │
│  │  │ (React) │  │ (TF/PyTorch)│  │ (React Native)        │        │   │
│  │  │ Realtime│  │ Stain class  │  │ Push alerts           │        │   │
│  │  │ History │  │ Fabric type  │  │ Scan & results        │        │   │
│  │  │ Energy  │  │ Fire risk    │  │ Cycle recommendations  │        │   │
│  │  │ Config  │  │ Dose optim   │  │ Fire alarm            │        │   │
│  │  └─────────┘  └──────────────┘  └───────────────────────┘        │   │
│  └───────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the Sub-GHz mesh to WiFi/BLE/cloud. Runs edge ML inference.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + ML + display; ESP32-C6 handles WiFi/BLE |
| Radio | SX1262 (868MHz) | Sub-GHz LoRa mesh to all nodes |
| Display | 2.8" IPS TFT (ILI9341) | Local status: cycle progress, lint risk gauge, alerts |
| Storage | W25Q256 32MB Flash + MicroSD | Data logging, OTA updates, ML model cache |
| Audio | Piezo buzzer + MAX9814 mic | Local fire alarm (85 dB) + ambient noise detection |
| Power | 5V USB-C + LiPo 2000mAh backup | Stays running during power outage (fire safety!) |
| Connectors | 4× I2C, 2× UART, 8× GPIO | Expansion |
| LEDs | RGB status LED | System state indication |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler for all nodes)
- Data aggregation and time-series buffering
- WiFi uplink to MQTT broker (QoS 1, TLS)
- BLE GATT server for mobile app
- TFT dashboard rendering (lint risk gauge, cycle status, detergent level)
- Local alarm triggers (piezo — 85 dB for fire alerts)
- OTA update distribution to all nodes
- TFLite Micro inference: lint fire risk prediction, cycle anomaly detection

### 2. Washer Node (1 per system)

Non-invasive washer monitor + automatic detergent dosing. Sits beside/behind the washing machine.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 | Handles sensors + pump + mesh |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Vibration | ADXL313 (3-axis accel) | Load imbalance, fabric type, cycle phase |
| Water Flow | YF-S201 hall-effect | Fill/drain monitoring, water usage tracking |
| Temperature | DS18B20 (waterproof) | Wash water temperature |
| Humidity | SHT40 | Ambient + leak humidity |
| Current | ACS712 (non-invasive clamp) | Washer cycle detection (motor on/off, spin phase) |
| Load Cell | 5kg + HX711 | Detergent reservoir weight (auto-dose measurement) |
| Pump | Kamoer NKP peristaltic | Precise detergent dispensing (±0.1 mL) |
| Power | 12V DC (pump) + 5V USB (MCU) | Split supply |

**Washer node firmware:**
- Detects washer cycle phases (fill → wash → rinse → spin → done) via current + vibration + flow
- Auto-dispenses detergent based on scan results (stain/fabric/soil) or user input
- Monitors detergent reservoir weight — alerts when low
- Vibration analysis: detects load imbalance (>0.4g) → recommends re-balancing
- Fabric type classification from vibration signature during wash
- Water usage tracking per cycle
- Reports cycle completion to hub → app notification
- Detects washer leaks via humidity spike
- Reports all telemetry to hub every 15 seconds

### 3. Dryer Node (1 per system) — ⚠️ SAFETY-CRITICAL

Lint fire prevention monitor. Non-invasive — attaches to the dryer's exterior and exhaust.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 | Handles all sensors + mesh |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Differential Pressure | MPXV7002DP | Lint trap clog detection (exhaust backpressure) |
| Exhaust Temp | K-type thermocouple + MAX6675 | Exhaust temperature (fire risk) — rated to 1024°C |
| Ambient Temp | DS18B20 | Room temperature baseline |
| Humidity | SHT40 | Exhaust moisture (dryness detection) |
| Vibration | ADXL313 | Drum imbalance, tumbler movement |
| Current | ACS712 (clamp) | Dryer on/off, heating element state |
| Smoke | MQ-2 (optional) | Smoke/combustible gas detection |
| Power | 5V USB | Low power, always-on |
| Enclosure | IP54, heat-resistant (PC) | Rated to 80°C continuous |

**Dryer node firmware (SAFETY-CRITICAL — always running):**
- **Lint trap clog detection:** Measures exhaust backpressure differential. Rising pressure = lint accumulation. Triggers "clean lint trap" alert at threshold, CRITICAL at high threshold.
- **Overheating detection:** K-type thermocouple on exhaust. Normal: 50-75°C. Warning: >85°C. CRITICAL: >95°C (fire imminent). Immediate alarm + dryer power advisory.
- **Dryness detection:** Humidity in exhaust drops when clothes are dry. Stops over-drying → saves 15-30% energy + prevents fabric damage.
- **Cycle detection:** Current sensor detects dryer on/off + heating element cycling.
- **Vibration monitoring:** Detects drum imbalance, tumbler belt slip.
- **Fire risk score:** Combines pressure + temp + humidity + usage history via ML → 0-1 risk score. >0.8 = immediate alarm + push notification + SMS.
- Reports every 10 seconds during operation, every 60 seconds idle.

### 4. Stain Scanner Node (1 per system, handheld)

Handheld multispectral garment scanner. Scan a garment before washing for stain/fabric ID and care recommendations.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 | Camera + display + mesh |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Camera | OV2640 (2MP) | Garment imaging |
| Illumination | White LED + UV-A 365nm + IR 940nm | Multispectral: stain chemistry, fiber fluorescence |
| Display | 1.3" ST7789 (240×240) | Scan results + recommendations |
| Battery | 1000mAh LiPo + MCP73831 | Portable, USB-C charging |
| Buttons | 3× capacitive touch | Scan / navigate / confirm |
| Power | LiPo + USB-C charging | ~1 week battery (100 scans/day) |

**Stain scanner firmware:**
- Captures multispectral image (white + UV + IR) of garment
- Runs on-device CNN (TFLite Micro) for fabric type classification: cotton, polyester, wool, silk, denim, blend, etc.
- Stain detection: UV fluorescence reveals biological stains (protein, blood, sweat); IR reveals oil-based stains
- Care label OCR (lightweight) — extracts wash/temp/symbols
- Sends results to hub → hub forwards to cloud ML for detailed stain ID + treatment
- Displays recommendation: stain type, pre-treatment, recommended cycle, temperature, detergent amount
- Battery management — deep sleep between scans (~5µA)

---

## Communication Protocol

### Sub-GHz Mesh (SX1262/61, 868MHz LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa SF7 (normal) / SF10 (fire alerts) |
| Bandwidth | 125 kHz |
| TX Power | +14 dBm (nodes) / +20 dBm (hub) |
| Range | 30m indoor (normal) / 200m (long range) |
| Protocol | Custom TDMA (hub is coordinator) |
| Slot Duration | 100ms per node |
| Cycle Time | 500ms (5 slots) |

### TDMA Frame Structure

```
│ SLOT 0 (HUB) │ SLOT 1 (WASHER) │ SLOT 2 (DRYER) │ SLOT 3 (SCANNER) │ SLOT 4 (CTRL) │
│   100ms      │    100ms        │    100ms       │    100ms        │   100ms      │
│
Total frame: 500ms
Slot 0: Hub broadcasts sync + commands
Slot 1: Washer node uplink telemetry
Slot 2: Dryer node uplink telemetry (SAFETY PRIORITY)
Slot 3: Scanner node uplink (when active)
Slot 4: Control/ACK/retransmit/OTA

Fire Alert Override:
  When dryer node detects fire risk >0.8, it immediately broadcasts
  BREATHING-ALERT-style override on SF10 (long range). Hub halts TDMA,
  relays to app + cloud + activates local piezo alarm.
```

### Mesh Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2) ]

TYPE values:
  0x01 = WASHER_DATA (vibration, flow, temp, humidity, detergent, cycle phase)
  0x02 = DRYER_DATA (pressure, exhaust_temp, humidity, ambient_temp, vibration, current)
  0x03 = SCAN_RESULT (fabric_type, stain_type, care_label, recommendation)
  0x04 = COMMAND (dose, alarm, override, cycle_select)
  0x05 = ACK
  0x06 = OTA_BLOCK (firmware update chunk)
  0x07 = CALIBRATION (sensor calibration data)
  0x08 = FIRE_ALERT (critical fire risk — highest priority, SF10 broadcast)
  0x09 = ENERGY_DATA (per-cycle energy + water usage)
  0x0A = HEARTBEAT
```

---

## AI / ML Pipeline

### 1. Lint Fire Risk Prediction (on-hub, TFLite Micro)

- Input: Rolling window of dryer telemetry (last 60 readings = 10 min during operation)
- Features: exhaust temp, differential pressure, humidity, ambient temp, current, vibration RMS
- Model: 1D-CNN + LSTM hybrid, INT8 quantized, ~95 KB
- Output: Fire risk score (0-1)
- Triggers: >0.6 = warning (clean lint trap soon), >0.8 = critical (clean NOW + check exhaust), >0.95 = EMERGENCY (immediate alarm, recommend dryer shutoff)
- Detects: lint accumulation, exhaust restriction, overheating, abnormal heating element behavior

### 2. Stain Classification (cloud + edge)

- Input: Multispectral image triple (white/UV/IR), 224×224 each
- Model: MobileNetV3-Small fine-tuned, 3-channel input (stacked spectral bands)
- Classes: clean, coffee, wine, blood, grease/oil, grass, ink, food, sweat, rust, unknown
- Output: stain class + confidence + recommended pre-treatment
- Deployed: cloud (full model) + scanner node (TFLite Micro, 8-class, ~180 KB)

### 3. Fabric Type Classification (edge, on scanner)

- Input: Multispectral image (UV fluorescence + white light texture)
- Model: Small CNN (4 conv layers), INT8, ~85 KB
- Classes: cotton, polyester, wool, silk, denim, nylon, linen, blend, unknown
- Output: fabric type + recommended wash cycle/temp/detergent
- UV fluorescence: wool/silk fluoresce differently than cotton/polyester

### 4. Detergent Dose Optimization (cloud)

- Input: fabric type, stain type, load weight (estimated), water hardness, detergent brand concentration
- Model: Gradient-boosted trees (XGBoost)
- Output: optimal detergent volume (mL) for this specific load
- Learns: your detergent's actual concentration, your machine's response curve

### 5. Energy & Water Optimization (cloud)

- Tracks per-cycle energy (from current sensor), water (from flow sensor), drying time
- Recommends: load size optimization, off-peak scheduling, cycle adjustments
- Predicts: monthly savings if recommendations followed

---

## Cloud Dashboard

React.js web app + Python FastAPI backend.

- Real-time laundry room status (washer/dryer cycle progress, lint risk gauge)
- Per-cycle energy + water usage with 30-day history
- Detergent inventory tracking + auto-reorder alerts
- Stain treatment library with before/after photos
- Fire safety log (all alerts, lint cleanings, maintenance)
- Fabric care database with wash recommendations
- OTA firmware update management
- Multi-machine support (laundromat, multi-family)

---

## Power Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    POWER DISTRIBUTION                     │
│                                                          │
│  HUB:    USB-C 5V ──► MCP73831 ──► LiPo 2000mAh         │
│          (backup: hub runs 8+ hrs on battery)            │
│          AP2112-3.3 (logic) + AP6212-1.8 (flash)         │
│                                                          │
│  WASHER: 12V DC (pump) ──► MP1584 buck ──► 5V (MCU)      │
│          USB-C 5V (MCU backup power)                     │
│                                                          │
│  DRYER:  USB-C 5V (always on — fire safety)               │
│          LiPo 500mAh backup (runs 2+ hrs if power out)    │
│                                                          │
│  SCANNER: LiPo 1000mAh ──► MCP73831 ──► USB-C charging    │
│           AP2112-3.3, deep sleep ~5µA                    │
└─────────────────────────────────────────────────────────┘
```

**Critical:** The dryer node has battery backup. If power goes out while the dryer was running (thermal mass still hot), the node keeps monitoring for fire risk.

---

## Bill of Materials (Summary)

| Node | Est. BOM Cost | Key Components |
|------|--------------|----------------|
| Hub | ~$22 | RP2040, ESP32-C6, SX1262, ILI9341 TFT, W25Q256, LiPo |
| Washer | ~$18 | ESP32-S3, SX1261, ADXL313, YF-S201, HX711, peristaltic pump |
| Dryer | ~$16 | ESP32-S3, SX1261, MPXV7002DP, MAX6675, K-type, SHT40, ADXL313 |
| Scanner | ~$14 | ESP32-S3, SX1261, OV2640, UV/IR LEDs, ST7789, LiPo |
| **Total** | **~$70** | Full 4-node system |

See `hardware/bom/` for detailed per-node BOMs.

---

## Assembly Overview

1. **Hub:** Place in laundry room, connect USB-C power. Has display + battery backup. Wall-mount or shelf.
2. **Washer node:** Attach vibration sensor to washer cabinet (magnetic mount). Clamp current sensor on washer power cord. Insert flow sensor in fill hose (or use non-invasive option). Place detergent reservoir on load cell platform, connect peristaltic pump output to washer dispenser drawer.
3. **Dryer node:** Attach differential pressure taps to exhaust duct (pre- and post-lint-trap). Tape K-type thermocouple to exhaust pipe exterior. Clamp current sensor on dryer power cord. USB-C powered.
4. **Stain scanner:** Handheld. Charge via USB-C. Point at garment, press scan button.

Detailed assembly in `docs/assembly_guide.md`.

---

## Safety Features

| Feature | Detection | Response |
|---------|-----------|----------|
| Lint trap clog | Rising exhaust backpressure | Warning alert → "Clean lint trap" |
| Exhaust restriction | High backpressure + rising temp | CRITICAL alert + app notification |
| Overheating | Exhaust temp >95°C | EMERGENCY alarm (85 dB) + SMS + recommend dryer shutoff |
| Fire risk (ML) | Combined sensor anomaly pattern | EMERGENCY — all alerts + local alarm |
| Washer leak | Humidity spike + flow anomaly | Warning alert + water shutoff advisory |
| Load imbalance | Vibration >0.4g during spin | Warning + "Rebalance load" |
| Power outage (dryer hot) | Current drops, temp still high | Warning + "Dryer was interrupted while hot — check for fire risk" |

---

## Getting Started

1. Set up the hub (USB-C power, connect to WiFi via mobile app over BLE)
2. Install washer node (non-invasive sensors, 15 min)
3. Install dryer node (exhaust sensors, 10 min — **do this first for fire safety**)
4. Charge stain scanner
5. Calibrate sensors (run `scripts/calibrate_sensors.py`)
6. Load detergent into reservoir
7. Start scanning + washing!

---

## Project Structure

```
wash-wise/
├── README.md              # This file
├── schematic/              # KiCad projects (one per node)
├── firmware/               # C source per node + shared common/
├── hardware/               # BOMs, enclosures
├── software/               # Cloud dashboard, ML pipeline, mobile app
├── scripts/                # Deployment, calibration
└── docs/                   # Architecture, API, protocol, assembly
```

---

## License

MIT — build it, sell it, improve it.

---

*Part of the [Devices](../README.md) collection — complex hardware+software systems that improve daily life.*