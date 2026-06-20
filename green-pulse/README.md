# GreenPulse

**AI-powered multi-node houseplant health monitoring & care companion system.** Continuously tracks soil moisture, light, temperature, and humidity per plant; scans leaves with multispectral imaging to detect disease, pests, and nutrient deficiency days before they're visible to the eye; learns each plant's species-specific water curve; and automates watering with per-zone solenoid valves — so anyone can keep a thriving indoor jungle without guesswork, dead plants, or overwatering. Solves the #1 houseplant problem: 70% of indoor plants die within the first year from overwatering, underwatering, wrong light, or undetected pests.

---

## What It Does

GreenPulse is a 4-node ambient + wearable system that turns a home full of houseplants into a continuously monitored, self-watering, disease-predicting care network:

1. **Tracks per-plant soil & environment** — a lightweight Plant Tag (nRF52832 + capacitive soil moisture + ambient light + SHT40 temp/humidity, Sub-GHz, coin-cell powered, one per pot) measures soil moisture, volumetric water content, ambient light (lux + PAR), temperature, and humidity every 15 minutes. Each tag is paired to a plant species profile via the app so care is species-specific, not generic.
2. **Learns each plant's water curve** — the cloud ML pipeline learns the drying rate of each individual pot (soil volume, plant size, species, ambient humidity, light exposure) and predicts exactly when each plant will hit its species-specific wilt threshold — so it can warn you to water *before* stress, never after, and never too much.
3. **Scans leaves for disease & pests** — a handheld Leaf Scanner node (ESP32-S3 + OV5640 multispectral camera + white/UV/NIR LED ring) captures multispectral leaf images. On-device + cloud ML classifies 40+ common diseases (powdery mildew, leaf spot, root rot signs, fungal rust), pests (spider mites, aphids, mealybugs, fungus gnats, thrips), and nutrient deficiencies (N/K/Fe/Mg chlorosis) — days before symptoms are visible to the human eye under white light alone (NIR + UV reveal pre-symptomatic stress).
4. **Automates watering** — a Water Valve node (ESP32-C6 + solenoid + flow sensor + pressure) attaches to a water reservoir or tap and waters groups of plants on schedule, with per-plant flow control via drip emitters. It only waters plants that actually need it (based on soil moisture + predicted wilt), preventing the #1 killer: overwatering. Flow sensors detect leaks or empty reservoirs.
5. **Identifies plant species** — point the Leaf Scanner at an unlabeled plant and the species-ID CNN (4,000+ species, PlantNet-derived) identifies it in 2 seconds, auto-loads the right care profile, and the tag is paired instantly.
6. **Predicts disease outbreaks** — a temporal CNN fuses 30 days of soil moisture, humidity, temperature, light, and leaf-scan history to forecast disease risk (e.g., "high powdery mildew risk in 3 days — reduce humidity" or "root rot risk elevated — skip next watering"). Humidity + moisture + temperature are the top disease drivers; GreenPulse watches them continuously.
7. **Optimizes light** — the tags measure lux + PAR throughout the day. The app tells you which plants are light-starved and suggests relocation, or triggers a smart-grow-light plug (smart-home integration) to supplement. Low light is the silent killer of indoor plants.
8. **Reminds & guides** — the app shows a per-plant dashboard (soil moisture %, light hours, last watering, disease risk, next-watering prediction) with push notifications: "Water your Monstera in 2 days," "Spider mites detected on your Calathea — see scan," "Your Fiddle Leaf Fig needs more light."
9. **Closes the loop automatically** — for users with the Water Valve node, the system waters autonomously: soil moisture drops below the species threshold → valve opens for the computed duration → flow sensor confirms delivery → moisture rises. The app logs every watering with before/after moisture so you see it worked. Travel for 3 weeks without losing a plant.
10. **Learns your home** — every home has different light (window direction, season), humidity (humidifier, bathroom), and temperature. GreenPulse builds a per-room microclimate model and recommends which plants will thrive where — so you buy the right plant for the right spot.

All Plant Tags communicate over a Sub-GHz mesh (868/915 MHz, long range, low power, coin-cell for 18+ months). The Leaf Scanner and Water Valve are WiFi. The Hub bridges Sub-GHz mesh to WiFi/cellular for cloud analytics, ML inference, and the mobile app.

### The Problem It Solves

- **Houseplants die — a lot:** The average home loses 70% of new houseplants in the first year. The causes are well-understood (overwatering, underwatering, wrong light, undetected pests/disease) but invisible until it's too late. By the time leaves yellow or wilt, root damage is often irreversible.
- **Overwatering is the #1 killer:** More plants die from overwatering (root rot) than underwatering. People water on a fixed schedule regardless of soil moisture, plant size, or season. GreenPulse measures soil moisture *per pot* and only waters what needs it.
- **Pests & disease are invisible early:** Spider mites, mealybugs, powdery mildew, and root rot are often undetectable under white light until damage is advanced. Multispectral (NIR + UV) imaging reveals stress *before* the human eye can see it — the same technology used in precision agriculture.
- **Light is the silent killer:** Most indoor plants are light-starved but don't show it for weeks (etiolation, slow decline). GreenPulse quantifies PAR (photosynthetically active radiation) per plant and tells you exactly which plants need more light and where to put them.
- **Care is generic, not species-specific:** A Monstera, a Calathea, and a cactus have opposite needs. Generic "water once a week" advice kills plants. GreenPulse loads species-specific care profiles (from a 4,000-species database) and adapts to your home's microclimate.
- **Travel = plant death:** Going away for 2-3 weeks usually means dead plants or begging a neighbor. GreenPulse's autonomous watering keeps every plant alive with zero intervention.
- **Nobody knows what plant they have:** Unlabeled plants get wrong care. GreenPulse identifies any plant in 2 seconds and loads the right profile automatically.

GreenPulse senses every plant continuously, predicts problems before they're visible, automates the watering, and tells you exactly what to do — so your indoor jungle thrives.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         GREENPULSE SYSTEM                                         │
│                                                                                    │
│  ┌─────────────────────┐ Sub-GHz mesh ┌──────────────────────┐                     │
│  │ PLANT TAG ×N         │◄───────────►│                        │                     │
│  │ (one per pot)        │  868/915MHz │     HUB NODE          │──── WiFi6 ──►Cloud │
│  │ nRF52832 + cap. soil │             │    (RP2040 +           │             Dashboard│
│  │ moisture + VEML7700  │             │     ESP32-C6)          │             + ML     │
│  │ light + SHT40 T/RH  │             │                        │             Pipeline│
│  │ + coin cell 18mo    │             │  Edge: species lookup   │             + Plant DB │
│  └─────────────────────┘             │   disease risk preview  │─── BLE ───► Mobile   │
│                                       │  TFT: plant grid view  │             (React   │
│  ┌─────────────────────┐             │   + watering queue      │              Native) │
│  │ LEAF SCANNER          │── WiFi6 ──►│                        │                     │
│  │ (handheld)            │            │  Speaker: care chimes   │                     │
│  │ ESP32-S3 + OV5640     │            └───────────┬────────────┘                     │
│  │ multispectral        │                        │ Sub-GHz                          │
│  │ + white/UV/NIR LED    │            ┌──────────▼─────────────┐                     │
│  │ Edge: species ID CNN  │            │  WATER VALVE NODE       │                     │
│  │ + disease pre-screen  │            │  (per zone, plugged)    │                     │
│  └─────────────────────┘            │  ESP32-C6 + solenoid    │                     │
│                                       │  + flow sensor + press  │                     │
│                                       │  per-plant drip emitters│                     │
│                                       └────────────────────────┘                     │
│                                                                                    │
│  ┌──────────────────────────────────────────────────────────────────────────────┐ │
│  │                    CLOUD / EDGE SOFTWARE                                       │ │
│  │  ┌──────────┐  ┌───────────────┐  ┌───────────────────────┐                 │ │
│  │  │Dashboard │  │ ML Pipeline   │  │ Mobile App            │                 │ │
│  │  │ (FastAPI)│  │ Disease CNN   │  │ Plant grid + moisture  │                 │ │
│  │  │ + Plant  │  │ Pest detector │  │ Disease risk + scans   │                 │ │
│  │  │   DB    │  │ Watering LSTM │  │ Watering log + schedule│                 │ │
│  │  │ Care    │  │ Species ID    │  │ "Water in 2 days"      │                 │ │
│  │  │ profiles│  │ Light optimizer│  │ Light relocation tips  │                 │ │
│  │  └──────────┘  └───────────────┘  └───────────────────────┘                 │ │
│  └──────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the Sub-GHz mesh to WiFi/cloud. Runs species lookup + disease-risk preview on edge, displays the plant grid + watering queue, and sends watering commands to the Water Valve node.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + edge ML + display; ESP32-C6 handles WiFi6 + Sub-GHz co-proc |
| Radio | nRF52832 (Sub-GHz via SX1262) + ESP32-C6 BLE | Sub-GHz mesh to plant tags; BLE to mobile app |
| Display | 3.5" IPS TFT (ILI9488) | Plant grid view (moisture bars per plant), watering queue, disease alerts |
| Storage | W25Q256 32MB Flash + MicroSD | Species DB cache, 30-day ring buffer of soil/light/temp data, OTA |
| RTC | PCF8563 + CR1220 | Timekeeping for watering schedules without WiFi |
| Audio | MAX98357A + 28mm speaker | Care chimes ("time to water"), disease alert tones |
| Power | 5V USB-C + LiPo 2500mAh backup | Runs through power outage; windowsill/desk-plugged normally |
| LEDs | WS2812 RGB + 4× SMD | System state: green=all good, amber=water soon, red=disease/pest |
| Connectors | 2× I2C, 2× UART, 6× GPIO | Expansion (additional valve zones, humidity controller) |

**Hub firmware responsibilities:**
- Maintain Sub-GHz mesh network with all plant tags + water valve
- Aggregate per-plant soil moisture, light, temp, humidity every 15 min
- Run edge disease-risk preview (heuristic + small TFLite Micro model) every hour
- Render plant grid (moisture bar per plant, color-coded status) on TFT
- Trigger watering: send valve command for plants below threshold
- Buffer 30 days of per-plant data locally (SD card) for cloud sync
- MQTT over WiFi to cloud; BLE to mobile app for instant alerts

### 2. Plant Tag Node (N per system — one per pot)

The workhorse. Planted in each pot. Measures soil + environment.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52832 | Sub-GHz mesh + sensor sampling + 18-month coin-cell life |
| Radio | SX1262 (Sub-GHz) | 868/915 MHz LoRa-style modulation, 50m indoor range, ultra-low-power |
| Soil Moisture | Capacitive (e.g., SMT100 or custom) | Volumetric water content 0-100%, no corrosion (capacitive, not resistive) |
| Light | VEML7700 | Ambient lux (0-120k) + IR — for light-hours tracking & PAR proxy |
| Temp/Humidity | SHT40 | ±0.2°C, ±1.8% RH — microclimate per plant |
| Power | CR2477 coin cell (1Ah) | 18+ months at 15-min sampling + Sub-GHz TX |
| LEDs | 1× SMD | Pairing + low-battery indicator |
| Antenna | PCB trace antenna (868/915 MHz) | Compact, no external antenna needed |

**Plant Tag firmware responsibilities:**
- Sample soil moisture (capacitive, 3 reads averaged), light, temp, humidity every 15 min
- Sub-GHz TX every 15 min (mesh relay extends range — tags forward neighbor data)
- Deep sleep (~14 µA) between samples for 18-month coin-cell life
- Pairing mode (button press → BLE advertisement for app pairing)
- Advertise low battery at 15% so you replace before death

### 3. Leaf Scanner Node (1 per system, handheld)

The diagnostician. Handheld multispectral leaf imager.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 (N16R8) | Camera driver + WiFi + on-device species-ID CNN (TFLite Micro) |
| Camera | OV5640 (5MP, autofocus) | High-res leaf capture with white + UV + NIR illumination |
| LED Ring | White 5500K + 365nm UV + 850nm NIR | Three illumination modes: visible (human-eye view), UV (pest/fungal fluorescence), NIR (pre-symptomatic stress) |
| Display | 1.3" OLED (SH1106) | Preview + scan result + species ID |
| Storage | MicroSD | Image archive (before/after disease progression) |
| Power | 18650 LiPo 2600mAh + USB-C | ~200 scans per charge; handheld |
| Buttons | 3× tactile | Capture / mode (white/UV/NIR) / identify |

**Leaf Scanner firmware responsibilities:**
- Capture multispectral image set (white + UV + NIR, 3 shots in 2 seconds)
- Run on-device species-ID CNN (4,000 species, ~2s inference) → display result
- Pre-screen for disease (TFLite Micro binary "healthy/suspect" model)
- Upload full multispectral set to cloud for high-res disease/pest CNN
- OLED preview + result display; SD card archive
- WiFi to hub/cloud (no Sub-GHz — image data is too large for mesh)

### 4. Water Valve Node (1+ per system, per zone)

The actuator. Automates watering per zone.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-C6 | Sub-GHz receiver (from hub) + WiFi + valve control |
| Radio | SX1262 (Sub-GHz) | Receives watering commands from hub; no need for WiFi if near hub |
| Valve | 12V latching solenoid (normally closed) | Low-power (only pulses to open/close), no continuous power |
| Flow Sensor | YF-S401 (hall-effect) | Confirms water delivery; detects leaks / empty reservoir |
| Pressure | MPX5010DP | Detects empty reservoir or blocked line |
| Pump | 5V mini diaphragm pump (optional) | For reservoir-fed systems (no tap water) |
| Power | 12V DC adapter + 18650 backup | Valve needs 12V; MCU on 3.3V LDO; battery for outage |
| Drip Emitters | Adjustable per-plant drippers | Per-plant flow rate via adjustable drippers (0.5-2 L/hr) |

**Water Valve firmware responsibilities:**
- Receive watering commands from hub (per-plant or per-zone, duration in seconds)
- Open latching solenoid for specified duration
- Monitor flow sensor: confirm liters delivered, detect leak (flow after close) or empty (no flow)
- Report watering result (liters, duration, status) to hub
- Safety: auto-close after max duration (10 min) regardless of command (flood prevention)
- Safety: never open if pressure sensor reports zero (empty reservoir)

---

## Communication Protocol

**Plant Tags ↔ Hub:** Sub-GHz mesh (868 MHz EU / 915 MHz US) via SX1262. Long-range (50m indoor), ultra-low-power (coin-cell), mesh-relay (tags forward neighbor packets). Binary protocol with CRC16.

**Leaf Scanner → Hub/Cloud:** WiFi6 (ESP32-S3). Image data is too large for Sub-GHz. Scanner uploads multispectral sets directly to cloud; hub gets a notification.

**Hub ↔ Water Valve:** Sub-GHz (SX1262). Low-latency watering commands.

**Hub → Cloud:** WiFi6 (ESP32-C6), MQTT over TLS. Per-plant telemetry, watering logs, disease scan results.

**Hub → Mobile App:** BLE 5.3 (instant alerts) + WiFi (full sync via cloud).

See [docs/protocol.md](docs/protocol.md) for the full frame specification.

---

## ML Pipeline

1. **Disease Classifier** — Multi-spectral (white+UV+NIR) leaf image CNN. 40+ disease classes across 20 common houseplant species. Trained on PlantVillage + PlantDoc + proprietary multispectral augmentation. Architecture: EfficientNet-Lite + 3-channel (white/UV/NIR) input → 40-class output. Deployed as cloud inference (too large for edge); edge gets a binary healthy/suspect pre-screen (TFLite Micro, <200KB).

2. **Pest Detector** — Object-detection (YOLOv8-nano) for spider mites, aphids, mealybugs, thrips, fungus gnats, whiteflies. Trained on IP102 + augmented synthetic pest overlays. Detects and counts pests per leaf; severity scoring.

3. **Watering Prediction** — Per-plant LSTM that learns the drying curve of each individual pot from 30 days of soil moisture + temp + humidity + light data. Predicts hours-to-wilt and optimal watering time. Personalized per pot (soil volume, plant size, species, microclimate).

4. **Species Identification** — MobileNetV3-based CNN trained on PlantNet 4,000-species dataset. Runs on-device (Leaf Scanner ESP32-S3, TFLite Micro, ~4MB int8). 2-second inference, top-5 results with confidence.

5. **Light Optimizer** — Per-plant light-hours analysis from VEML7700 lux data. Computes daily light integral (DLI) per plant and compares to species requirements; recommends relocation or grow-light supplementation.

See [software/ml-pipeline/](software/ml-pipeline/) for training scripts.

---

## Mobile App

React Native app with:
- **Plant Grid** — every plant with moisture %, status color, next-watering countdown
- **Plant Detail** — 30-day moisture chart, light hours, temp/humidity, watering log, scan history
- **Scan** — trigger Leaf Scanner, view disease/pest results with annotated leaf images
- **Watering** — manual watering log, auto-water toggle, schedule override
- **Care Tips** — species-specific care profile, light relocation suggestions, disease prevention
- **Alerts** — "Water Monstera in 2 days," "Spider mites on Calathea," "Tag #3 battery low"

See [software/mobile-app/](software/mobile-app/) for source.

---

## BOMs

- [Hub Node BOM](hardware/bom/hub_node_bom.csv) — ~$42 (Qty1), ~$24 (10k)
- [Plant Tag BOM](hardware/bom/plant_tag_bom.csv) — ~$14 (Qty1), ~$6.50 (10k)
- [Leaf Scanner BOM](hardware/bom/leaf_scanner_bom.csv) — ~$38 (Qty1), ~$22 (10k)
- [Water Valve BOM](hardware/bom/water_valve_bom.csv) — ~$28 (Qty1), ~$16 (10k)

A starter system (1 Hub + 5 Plant Tags + 1 Leaf Scanner + 1 Water Valve) ≈ **$190 retail**, expandable to 50+ tags.

---

## Power Architecture

- **Hub:** USB-C 5V + LiPo 2500mAh backup (~6 hr outage)
- **Plant Tags:** CR2477 coin cell (1 Ah) → 18+ months at 15-min sampling + Sub-GHz TX (14 µA sleep, ~25 mA active for 200ms)
- **Leaf Scanner:** 18650 2600mAh + USB-C → ~200 scans/charge
- **Water Valve:** 12V DC adapter + 18650 backup → valve solenoid on 12V, MCU on 3.3V LDO

---

## Privacy

- No microphones, no cameras in living spaces (Leaf Scanner is handheld, user-initiated, no always-on imaging)
- All data encrypted in transit (TLS) and at rest
- Plant data is yours; no third-party sharing
- Scanner images stored locally on SD + optional cloud backup (user-controlled)

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) collection — a new complex device system every 24 hours.*