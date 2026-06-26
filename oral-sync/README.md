# OralSync — AI-Powered Oral Health & Cavity Prevention System

> **Catch cavities before they exist — the world's first whole-mouth oral health intelligence system.**

OralSync is a multi-node oral health monitoring, prediction, and coaching system for families, orthodontic patients, and anyone who wants to stop drill-and-fill dentistry before it starts. It tracks your brushing technique in real time, maps plaque biofilm across every tooth surface with multispectral fluorescence imaging, measures salivary pH and nitrite daily, and predicts caries (cavity) risk 90 days out — coaching you, tooth by tooth, to a healthier mouth.

## Why OralSync?

- **Oral disease is the #1 chronic disease on Earth** — 3.5 billion people affected, more than diabetes, asthma, and epilepsy combined
- **2.3 billion people** have untreated caries in permanent teeth; **530 million children** in primary teeth
- **Gum disease** affects ~50% of adults and is linked to heart disease, type-2 diabetes, Alzheimer's, and preterm birth
- People **miss 30–40% of tooth surfaces** when brushing and never know it — plaque hardens into tartar in 24–72h
- **White-spot lesions** (early caries) are reversible with remineralization if caught early — irreversible once cavitated; no home system detects them
- Dentistry is **reactive**: by the time you feel pain, the tooth needs drilling. Preventive dentistry is data-starved.
- Salivary pH and buffering capacity are strong caries-risk predictors but never measured at home
- **No integrated system** exists that fuses motion-tracked brushing + multispectral plaque imaging + salivary biochemistry + longitudinal ML forecasting

## System Architecture

```
┌─────────────┐     BLE 5.0 (OSMP)       ┌──────────────┐
│  Smart      │◄─────────────────────────►│              │
│  Toothbrush │                            │              │
│  Node ×N    │                            │              │
└─────────────┘                            │   OralSync   │
┌─────────────┐     BLE 5.0 (OSMP)       │    Hub       │
│  Plaque     │◄─────────────────────────►│  (RP2040 +  │
│  Scanner    │                            │  ESP32-C3)  │
└─────────────┘                            │              │
┌─────────────┐     BLE 5.0 (OSMP)       │  5" Mirror   │
│  Saliva     │◄─────────────────────────►│  Display    │
│  Sensor     │                            └──────┬───────┘
└─────────────┘                                   │
                                           Wi-Fi / MQTT
                                                  │
                                         ┌────────▼────────┐
                                         │  Cloud Dashboard │
                                         │  (FastAPI + ML)  │
                                         └────────┬─────────┘
                                                  │
                                           REST / WebSocket
                                                  │
                                         ┌────────▼────────┐
                                         │  Mobile App      │
                                         │  (React Native)  │
                                         └──────────────────┘
```

## Nodes

### 1. OralSync Hub (Smart Mirror Hub)
- **SoC**: RP2040 (real-time coordination, display driver, TFLite Micro inference) + ESP32-C3 (Wi-Fi/MQTT bridge + BLE 5.0 central)
- **Role**: Bathroom-mirror-mounted hub. Coordinates all nodes, renders the live plaque heatmap on a 5" IPS display, runs on-device TFLite Micro for plaque segmentation preview and brushing-coach state machine, provides voice coaching via I2S mic + speaker, Wi-Fi uplink to cloud
- **Sensors**:
  - SHT40 ambient temp/humidity (bathroom humidity affects oral microbiome & salivary flow)
  - VEML6075 UV index (sunlight/vitamin D correlation with oral health)
  - SPH0645LM4H I2S MEMS mic (voice commands / coaching)
- **Actuators**: 1.5 W class-D speaker (I2S, MAX98357A), NeoPixel ring (16 RGB) for brushing pace + alert cues
- **Comms**: BLE 5.0 central (ESP32-C3, OSMP) to Toothbrush/Scanner/Saliva nodes, Wi-Fi 4 to cloud MQTT
- **Display**: 5.0" 720×1280 IPS LCD via RP2040 SPI (ST7701 driver), shows plaque heatmap, brushing zone coverage, risk gauge
- **Power**: 5V USB-C, ~200 mA avg (display + BLE + Wi-Fi)

### 2. Smart Toothbrush Node (×N, one per family member)
- **SoC**: nRF52840 (Cortex-M4F, BLE 5.0, ultra-low-power)
- **Role**: Tracks every brushing session — 6-DoF motion to determine which tooth/sextant/surface is being brushed, pressure to detect over-brushing (gum recession) and under-brushing, 2-minute timer with quadrant pacing
- **Sensors**:
  - ICM-42688-P 6-axis IMU (±16 g accel, ±2000 dps gyro) — brush orientation → sextant/face (occlusal/buccal/lingual)
  - Interlink FSR 402 force-sensing resistor at the brush neck — pressure 0.1–10 N (over-pressure >3.5 N flags recession risk)
  - nRF52840 internal RTC for session timing
- **Actuators**: Linear resonant actuator (LRA, Pico Vibe) for quadrant-pace haptics + over-pressure warning
- **Comms**: BLE 5.0 peripheral (OSMP) to Hub
- **Power**: 500 mAh Li-Po + MCP73831 USB-C charger, ~30-day battery (active only ~4 min/day), IP67 wand
- **Enclosure**: Clip-on sensor collar for manual brushes, or OEM sonic-handle form factor
- **ML**: Brushing technique classifier runs on Hub (IMU stream → 8-class: Bass/Fones/Stillman/Scrub/Floss-pick/etc.) via TFLite Micro

### 3. Plaque Scanner (handheld, periodic use)
- **SoC**: ESP32-S3 (dual-core 240 MHz, vector instructions, camera DVP, ML-capable)
- **Role**: Intraoral multispectral imaging — detects plaque biofilm (fluoresces under 405 nm violet), gingival inflammation (redness + 660/850 nm ratio), early white-spot caries (demineralization spectral signature at 405/470/525/660/850 nm), calculus/tartar, staining
- **Sensors**:
  - OV5640 autofocus camera (5 MP, DVP, with disposable cheek-retractor + lens cover)
  - Multispectral LED ring: 405 nm violet (plaque fluorescence, porphyrin from *Porphyromonas*), 470 nm blue, 525 nm green, 660 nm red, 850 nm NIR (inflammation / subsurface demin)
  - VL53L1X ToF (focus distance, 30–200 mm intraoral)
  - SHT40 (intraoral breath humidity proxy)
  - Buzzer for "hold still" cue
- **Comms**: BLE 5.0 to Hub, Wi-Fi for OTA
- **Power**: 1200 mAh Li-Po, USB-C, ~1 week periodic use
- **Display**: 2.0" 320×240 IPS (ST7789) live preview + plaque heatmap overlay
- **ML**: On-device TFLite — plaque-segmentation U-Net-tiny (per-pixel plaque mask), gingivitis classifier (Healthy/Mild/Moderate/Severe), caries white-spot detector. Feature embeddings sent to cloud for longitudinal lesion tracking.

### 4. Saliva Sensor (daily benchtop)
- **SoC**: STM32L432KC (ultra-low-power Cortex-M4F) + SPBTLE-RF (BlueNRG-2 SPI BLE module)
- **Role**: Daily salivary biochemistry — pH, nitrite (NO2⁻), and buffer capacity. Salivary pH <6.2 indicates demineralization risk; nitrite reflects oral nitrate-reducing bacteria (cardioprotective & periodontal health proxy); low buffer capacity → high caries risk.
- **Sensors**:
  - ISFET pH sensor (Hanna-style replaceable tip, pH 0–14, ±0.05) — salivary pH
  - Electrochemical nitrite sensor (3-electrode, NO2⁻ 1–100 µM, ±2 µM) — replaces dip-strip with calibrated amperometry
  - DS18B20 sample temperature (compensation)
  - Load cell (HX711, 5 kg) — buffer-capacity titration micro-volume measurement
- **Comms**: BLE 5.0 (BlueNRG-2) to Hub
- **Power**: CR2477 coin-cell + boost (low duty, 1 reading/day), ~6-month battery
- **Enclosure**: Benchtop dock with disposable cuvette tray; pH/nitrite tips are consumable (replace monthly)

## Communication Protocol

All nodes use the **OralSync Sync Protocol (OSMP)** over BLE 5.0 GATT, star topology (Hub = central):

- **PHY**: BLE 5.0, 2 Mbps coded PHY when far, 1 Mbps default
- **Topology**: Star — Hub central, all others peripherals
- **GATT**: Custom service UUID `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` (Nordic-UART-like)
  - TX char `...0003...` (node → hub, Notify)
  - RX char `...0002...` (hub → node, Write)
- **Frame**: `[SOP(0xAA)] [LEN(1)] [TYPE(1)] [SEQ(1)] [PAYLOAD(0-180)] [CRC16(2)]`
- **Encryption**: AES-128-CCM (per-node key provisioned at pairing)
- **Message types**: HELLO, PAIR, SESSION_START, IMU_SAMPLE, PRESSURE_SAMPLE, SCAN_FRAME, SALIVA_READING, COACH_CUE, ACK, OTA_CHUNK
- **Duty cycle**: Toothbrush streams IMU+pressure at 50 Hz during a session; Scanner streams scan frames at 2 Hz; Saliva reports once per reading; Hub uplinks to cloud every session end + hourly health rollup
- **Cloud uplink**: MQTT over TLS to `mqtt.oralsync.cloud`, topics `oralsync/<home_id>/<node_id>/telemetry` & `.../event`

## Firmware

Each node runs bare-metal C:

- **Common**: `common/osmp.h` (OSMP frame encode/decode + CRC16), `common/osmp_sensors.h` (shared sensor driver abstractions)
- **Hub**: `hub/main.c` (RP2040 coordination + display), `hub/coach.c` (TFLite Micro brushing coach), `hub/ble_central.c` (ESP32-C3 BLE), `hub/wifi_bridge.c` (MQTT)
- **Toothbrush**: `toothbrush/main.c` (nRF52840), `toothbrush/imu.c` (ICM-42688), `toothbrush/pressure.c` (FSR), `toothbrush/haptics.c` (LRA)
- **Scanner**: `scanner/main.c` (ESP32-S3), `scanner/camera.c` (OV5640), `scanner/spectral.c` (LED ring sequencing), `scanner/tflite_plaque.c` (U-Net-tiny)
- **Saliva**: `saliva_patch/main.c` (STM32L432), `saliva_patch/ph.c` (ISFET), `saliva_patch/nitrite.c` (amperometry), `saliva_patch/ble.c` (BlueNRG-2)

## Cloud / Edge Software

### FastAPI Dashboard
- REST + WebSocket API for real-time session data, scan images, saliva readings
- PostgreSQL timeseries (TimescaleDB) — per-tooth, per-surface, per-session history
- Oral-health timeline: plaque heatmap history, lesion change tracking, pH/nitrite trends
- Alert engine: new white-spot lesion, plaque coverage >40%, sustained acidic pH <6.2, over-pressure events, missed surfaces
- Caries-risk score (0–100) per tooth, 90-day forecast
- Dentist-ready clinical report export (PDF, per-quadrant findings)
- Multi-user household support with per-person profiles & orthodontic (braces) mode

### ML Pipeline (6 models)
1. **Brushing-technique classifier** — IMU 6-DoF stream → 8 techniques (Bass / Modified-Bass / Fones / Stillman / Scrub / Charter / Floss-pick / Sonic). CNN (1D Conv) on 2 s windows.
2. **Plaque-segmentation U-Net-tiny** — multispectral image → per-pixel plaque mask. Outputs plaque % per surface.
3. **Gingivitis classifier** — multispectral + red/NIR ratio → Healthy/Mild/Moderate/Severe per sextant. MobileNetV3-tiny backbone.
4. **Caries white-spot detector** — 405/470/525/660/850 spectral signature → lesion probability + localization (YOLOv8-nano on 405 nm + NIR difference image).
5. **Plaque-growth LSTM** — per-tooth plaque history + brushing + diet proxies → 72 h plaque regrowth forecast.
6. **Caries-risk forecaster** — fuses plaque trends, pH/nitrite, brushing coverage, lesion history, age → 90-day caries risk per tooth. Temporal-gradient-boosted (LightGBM) on per-tooth weekly features.

## Mobile App (React Native)

- **Dashboard**: whole-mouth 3D tooth map, color-coded plaque/risk, today's score
- **Brush Session**: live zone-coverage map, technique feedback, post-session plaque-coverage delta
- **Scan**: review plaque heatmap, lesion tracking timeline, before/after slider
- **Risk**: 90-day caries-risk forecast per tooth, trend charts for pH/nitrite/plaque
- **History**: session log, saliva readings, lesion history, dental-visit annotations
- **Settings**: household profiles, brushing goals, orthodontic mode, dentist report export, consumable tip reorder

## Bill of Materials (summary)

See `hardware/bom/*.csv` for full per-node BOMs. Highlights:

| Node | Key ICs | Est. BOM (qty 1k) |
|------|---------|-------------------|
| Hub | RP2040, ESP32-C3, ST7701 LCD, SHT40, VEML6075, MAX98357A | $24.80 |
| Toothbrush | nRF52840, ICM-42688-P, FSR 402, LRA, MCP73831 | $11.20 |
| Plaque Scanner | ESP32-S3, OV5640, VL53L1X, 5× spectral LEDs, ST7789 | $32.40 |
| Saliva Sensor | STM32L432, SPBTLE-RF, ISFET pH, NO2⁻ electrode, HX711 | $19.60 |

## Power Architecture

- **Hub**: 5 V USB-C → 3.3 V (MP2359 buck) + 1.8 V (LCD), always-on
- **Toothbrush**: 3.7 V Li-Po → 3.3 V (nRF52833 LDO), MCP73831 charger, <1 mA sleep
- **Scanner**: 3.7 V Li-Po → 3.3 V + 5 V (camera), TP4056 charger, deep-sleep between scans
- **Saliva Sensor**: 3.0 V CR2477 → 3.3 V boost (TPS61099), <5 µA sleep, wakes once daily

## What Makes This Different

Existing smart toothbrushes (Oral-B, Philips, Quip) track duration and vague "zones" — none image your actual plaque, none measure saliva chemistry, none forecast cavities. Dental cameras exist but don't segment plaque or track lesions over time. OralSync is the first system to close the loop: **measure → image → predict → coach**, tooth by tooth, surface by surface.

## Regulatory Note

OralSync is a **wellness & coaching device**, not a diagnostic medical device. It flags risk and coaches behavior; it does not diagnose caries or periodontitis. Lesion findings are advisory and intended to prompt a dental visit. The cloud risk model is CE/FDA wellness-class intended.

## License

MIT — build it, sell it, improve it.

---

*Invented by [jayis1](https://github.com/jayis1). Part of the [Devices](https://github.com/jayis1/Devices) collection.*