# PoolSync — AI-Powered Pool & Spa Health Intelligence System

> Stop guessing at water chemistry. Stop wasting chemicals. Stop algae before it starts.
> PoolSync is a 4-node pool monitoring, chemistry automation, and safety system that keeps your pool crystal-clear with 40% less chemicals and 30% less energy.

## What It Does

PoolSync continuously monitors your pool's water chemistry, visual clarity, equipment health, and energy consumption — then automatically doses chemicals, schedules pump runs, predicts algae outbreaks 3 days in advance, and alerts you to safety hazards.

### The Problem

- **Pool ownership is expensive** — $1,200–$2,500/yr in chemicals, energy, and maintenance
- **Water chemistry is hard** — pH drift, chlorine demand, alkalinity collapse, cyanuric acid buildup
- **Algae sneaks up** — by the time you see green, you're already 3 days behind
- **Energy waste is invisible** — pumps running too long, heaters overshooting, off-peak opportunities missed
- **Safety risks are real** — entrapment, electrical hazards, unsupervised access, chemical off-gassing

### The Solution

PoolSync replaces test strips, guesswork, and reactive panic with a fully automated, AI-driven pool management system that monitors, predicts, and acts.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        PoolSync Cloud                           │
│  FastAPI Dashboard · ML Pipeline · Weather Integration           │
│  Algae Forecast · Chemistry Optimizer · Energy Advisor           │
└──────────┬───────────────────────────────────┬──────────────────┘
           │ Wi-Fi / MQTT                      │
┌──────────▼───────────────────────────────────▼──────────────────┐
│                     PoolSync Hub (RP2040 + ESP32)               │
│  Zone Coordinator · Sub-GHz Radio · BLE 5.0 · Wi-Fi Gateway    │
│  Local Rules Engine · Data Aggregation · Edge ML Inference     │
└──┬────────────┬─────────────────┬──────────────┬───────────────┘
   │ Sub-GHz    │ Sub-GHz         │ Sub-GHz      │ Sub-GHz
   │ 868 MHz    │ 868 MHz         │ 868 MHz      │ 868 MHz
┌──▼────────┐ ┌▼──────────────┐ ┌▼────────────┐ ┌▼──────────────┐
│ Chemistry │ │ Pool Camera   │ │ Equipment   │ │ Solar Monitor │
│ Probe ×N  │ │               │ │ Controller  │ │   (optional)  │
│ pH/ORP/   │ │ 4K RGB + IR   │ │ Pump/Heat/  │ │ Irradiance +  │
│ Cl/Temp   │ │ Water Clarity │ │ Valve/Flow   │ │ Panel Monitor │
│ Conduct.  │ │ Algae Detect  │ │ Auto-Dose   │ │ MPPT Optimize │
└───────────┘ └───────────────┘ └─────────────┘ └───────────────┘
```

---

## Hardware Nodes

### 1. PoolSync Hub
**The brain** — coordinates all nodes, runs local rules, bridges to cloud.

| Component | Part | Notes |
|-----------|------|-------|
| Main MCU | RP2040 | Dual-core Cortex-M0+, 133 MHz |
| Wireless MCU | ESP32-S3 | Wi-Fi 6, BLE 5.0, 240 MHz |
| Sub-GHz Radio | SX1262 | 868 MHz LoRa, 2 km range |
| Display | 2.8" IPS LCD | ILI9341, 320×240, pool status dashboard |
| Power | 5V/3A USB-C | PoE optional |
| Enclosure | IP65 wall-mount | Indoor/covered outdoor |

### 2. Chemistry Probe ×N (1–3 per pool)
**The chemist** — continuous water chemistry monitoring, submerged in pool.

| Component | Part | Notes |
|-----------|------|-------|
| MCU | STM32L476RG | Ultra-low-power Cortex-M4, 1 MB flash |
| Sub-GHz Radio | SX1262 | 868 MHz, sleeps between readings |
| pH Sensor | ISFET pH probe | 0–14 pH, ±0.01 accuracy |
| ORP Sensor | Platinum ORP electrode | ±1 mV, chlorine proxy |
| Free Chlorine | Amperometric sensor | 0–10 ppm, ±0.02 ppm |
| Temperature | DS18B20 | Waterproof, -55–125°C |
| Conductivity | Inductive toroidal | 0–100 mS/cm, TDS proxy |
| Turbidity | TSL2591 light sensor | Paired IR LED, 0–1000 NTU |
| Power | 3× AA LiFeS2 | 18-month battery, waterproof housing |
| Enclosure | IP68 titanium-body | Submersible, chemical-resistant |

### 3. Pool Camera
**The eyes** — water clarity assessment, algae detection, safety monitoring.

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3 | Dual-core 240 MHz, Wi-Fi + BLE |
| Camera | IMX477R | 12 MP, 4K, IR-cut + IR-LED for night |
| Light Sensor | TSL2591 | Ambient light for exposure control |
| PIR Motion | AM312 | Human detection for safety alerts |
| Speaker | MAX98357A | 3W class-D, verbal warnings |
| Storage | 32 GB eMMC | Edge image buffer |
| Power | 5V/2A solar panel + LiPo 3.7V/4000 mAh | Self-sustaining outdoor |
| Enclosure | IP66 dome camera housing | Pool-side pole mount |

### 4. Equipment Controller
**The hands** — controls pump, heater, valves, and auto-chemical dosing.

| Component | Part | Notes |
|-----------|------|-------|
| MCU | STM32F407VG | Cortex-M4, 168 MHz, plenty of I/O |
| Sub-GHz Radio | SX1262 | 868 MHz to hub |
| Relay Array | 8× SPDT 16A | Pump, heater, lights, valves |
| Peristaltic Pumps | 3× stepper-driven | Acid, chlorine, clarifier dosing |
| Flow Sensor | YF-S201 | 1–30 L/min, dosing verification |
| Pressure Sensor | MPX5010DP | Filter pressure, clog detection |
| Power | 24VAC→5V/3A | Pool equipment power tap |
| Enclosure | NEMA 4X | Weatherproof, lockable |
| Safety | GFCI + entrapment sensor | VGB-compliant safety interlock |

### 5. Solar Monitor (Optional Add-on)
**The energy optimizer** — solar panel monitoring for solar-heated pools.

| Component | Part | Notes |
|-----------|------|-------|
| MCU | STM32L476RG | Low-power Cortex-M4 |
| Sub-GHz Radio | SX1262 | 868 MHz |
| Irradiance | ML8511 | UV intensity for solar gain |
| Current | ACS712-30A | Solar pump current |
| Temperature | DS18B20 ×2 | Panel + roof temp |
| Power | Solar + LiPo | Self-powered |

---

## Communication Protocol

**PoolSync Protocol (PSP)** — layered on Sub-GHz LoRa + Wi-Fi MQTT:

| Layer | Protocol | Notes |
|-------|----------|-------|
| Physical | SX1262 868 MHz LoRa / Wi-Fi 6 | 2 km Sub-GHz, local Wi-Fi |
| Transport | MQTT (cloud) / Custom TDMA (Sub-GHz) | QoS 1 for commands |
| Application | PSP binary frames | Little-endian, CRC16 |
| Security | AES-128-GCM | Per-node keys, replay protection |

### PSP Frame Format
```
┌──────┬──────┬──────┬──────┬─────────┬────────┬──────┬──────┐
│ PREAMBLE │ SYNC │ LEN  │ SRC  │ DST     │ TYPE   │ PAY  │ CRC  │
│ 2 bytes  │ 2B   │ 2B   │ 2B   │ 2B     │ 1B     │ 0-200│ 2B   │
└──────┴──────┴──────┴──────┴─────────┴────────┴──────┴──────┘
```

### Message Types
| Type | ID | Direction | Purpose |
|------|----|-----------|---------|
| CHEM_DATA | 0x01 | Probe→Hub | pH, ORP, Cl, temp, conductivity, turbidity |
| IMAGE_DATA | 0x02 | Camera→Hub | Compressed water clarity image metadata |
| IMAGE_UPLOAD | 0x03 | Hub→Camera | Trigger full image upload over Wi-Fi |
| EQUIP_STATUS | 0x04 | Equip→Hub | Pump/heater/valve status, flow, pressure |
| DOSE_COMMAND | 0x05 | Hub→Equip | Chemical dosing command (ml, pump) |
| EQUIP_COMMAND | 0x06 | Hub→Equip | Pump/heater/valve control |
| SOLAR_DATA | 0x07 | Solar→Hub | Irradiance, current, panel temp |
| ALARM | 0x08 | Any→Hub | Safety alarm (entrapment, GFCI, access) |
| HEARTBEAT | 0x10 | Any↔Hub | Keep-alive, battery, RSSI |
| OTA_START | 0x20 | Hub→Any | Begin over-the-air firmware update |
| OTA_CHUNK | 0x21 | Hub→Any | Firmware chunk transfer |
| OTA_DONE | 0x22 | Any→Hub | Update verification |

---

## Firmware

### Hub (`firmware/hub/`)
- RP2040 core: Sub-GHz radio management, local rules engine, display driver
- ESP32-S3: Wi-Fi/MQTT bridge, BLE provisioning, edge ML inference
- Rules engine: pH drift compensation, freeze protection, dosing safety interlocks
- OTA update server for all nodes

### Chemistry Probe (`firmware/chemistry_probe/`)
- STM32L476 low-power operation: wake every 5 min, read sensors, transmit, sleep
- Sensor excitation sequencing (pH → ORP → Cl → temp → conductivity)
- ISFET calibration with 2-point pH buffer auto-calibration
- Battery management with 3-year projected life

### Pool Camera (`firmware/pool_camera/`)
- ESP32-S3 camera driver with day/night IR switching
- On-device water clarity scoring (histogram analysis + green channel detection)
- PIR-triggered safety capture (unsupervised access near pool)
- Motion-triggered image upload over Wi-Fi

### Equipment Controller (`firmware/equipment_controller/`)
- STM32F407 real-time control: relay scheduling, peristaltic pump stepper driving
- Flow verification after dosing commands (confirm chemical delivery)
- GFCI monitoring + entrapment pressure differential detection
- Safety interlock: pump shutoff if entrapment or GFCI fault detected

### Common (`firmware/common/`)
- PSP protocol frame encode/decode
- AES-128-GCM encryption/decryption
- CRC16 calculation
- Sensor abstraction layer
- Ring buffer, event queue, logging

---

## Software

### Dashboard (`software/dashboard/`)
FastAPI backend with:
- Real-time pool chemistry dashboard (pH, chlorine, ORP, temp, conductivity, turbidity)
- Equipment status and control (pump schedule, heater setpoint, valve positions)
- Chemical dosing history and consumption tracking
- Algae risk forecast (3-day) with confidence intervals
- Energy usage analytics with solar optimization
- Safety event log with push notifications
- Weather integration (NWS/OWM) for rain/UV/storm impact
- User management with pool service professional sharing

### ML Pipeline (`software/ml-pipeline/`)
6-model pipeline:
1. **AlgaeNet** — 3-day algae outbreak forecast (LSTM with chemistry + weather + clarity)
2. **ChemBalance** — optimal dosing calculator (gradient-boosted trees)
3. **ClearWater** — water clarity classifier from images (MobileNetV3 + custom head)
4. **EnergyOpt** — pump/heater schedule optimizer (reinforcement learning, DQN)
5. **AnomalyDetect** — equipment fault detection (autoencoder on vibration/flow/pressure)
6. **SafetyNet** — pool access detection + distress recognition (YOLOv8-nano + pose)

### Mobile App (`software/mobile-app/`)
React Native cross-platform:
- Pool health score (0–100) with trend arrow
- Real-time chemistry readings with ideal range overlays
- One-tap "shock treatment" and "vacation mode" actions
- Push notifications for algae risk, safety alerts, chemical low
- Equipment scheduling with drag-and-drop calendar
- Service professional portal for sharing reports
- Photo capture for manual water clarity check

---

## Key Innovations

1. **ISFET pH measurement** — solid-state pH sensor (no glass bulb) survives pool chemistry and lasts 3+ years
2. **Amperometric free chlorine** — direct ppm measurement (not ORP proxy) with ±0.02 ppm accuracy
3. **Turbidity + color dual measurement** — IR LED + TSL2591 gives NTU and green channel for early algae
4. **Automatic chemical dosing** — peristaltic pumps with flow verification, never overdose
5. **3-day algae forecast** — ML model fuses chemistry, weather, and visual clarity to predict outbreaks
6. **Energy optimization** — DQN learns optimal pump/heater schedules considering solar, TOU rates, and bather load
7. **VGB safety interlock** — entrapment pressure detection, GFCI monitoring, unsupervised access alerts
8. **Weather-aware chemistry** — rain dilutes chlorine, heat increases demand; system adjusts proactively

---

## Typical Use Cases

- **Residential pool owners** — set-and-forget pool maintenance, 40% less chemical spend
- **Pool service professionals** — manage 50+ pools from one dashboard, route optimization
- **Hotel/resort operators** — guest safety, health department compliance documentation
- **Public pool operators** — VGB compliance, hourly chemistry logging, health department reports
- **New pool construction** — integrate PoolSync during build for smart pool from day one

---

## Cost Targets

| Node | BOM Cost (1K units) | Retail Target |
|------|---------------------|---------------|
| Hub | $42 | $149 |
| Chemistry Probe | $65 | $199 |
| Pool Camera | $38 | $129 |
| Equipment Controller | $55 | $179 |
| Solar Monitor | $28 | $89 |
| Full System (4-node) | $200 | $649 |

---

## License

MIT — build it, sell it, improve it.