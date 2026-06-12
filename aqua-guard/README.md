# Aqua Guard

**AI-powered multi-node aquarium ecosystem monitor and autoregulator.** Keeps your fish alive, your water perfect, and your tank beautiful — autonomously.

---

## What It Does

Aqua Guard is a 4-node system that turns any aquarium into a self-regulating ecosystem:

1. **Monitors** water quality continuously (pH, temp, ammonia, nitrite, nitrate, dissolved oxygen, TDS, turbidity)
2. **Doses** conditioners, fertilizers, and medications automatically via peristaltic pumps
3. **Controls** lighting spectrum and intensity on a natural circadian schedule
4. **Feeds** fish on AI-optimized schedules based on behavior analysis
5. **Alerts** you before problems happen — not after your fish are dead
6. **Learns** your specific tank's ecosystem over time (transfer learning on your data)

All nodes communicate over a dedicated Sub-GHz mesh network (no WiFi dependency for critical life-support functions). A hub node bridges to WiFi/cloud for the dashboard and mobile app.

### The Problem It Solves

- 73% of new aquarium owners quit within the first year — mostly because of water quality crashes they didn't see coming
- Daily testing is tedious and most people skip it
- Overfeeding kills more fish than disease
- Wrong dosing of chemicals does more harm than good
- Lighting schedules affect fish health, plant growth, and algae — hard to get right

Aqua Guard automates all of this. You set up the hardware, calibrate once, and it runs your tank better than most experts.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        AQUA GUARD SYSTEM                            │
│                                                                     │
│  ┌──────────────┐   Sub-GHz    ┌──────────────┐                    │
│  │  SENSOR NODE │◄───────────►│              │                    │
│  │  (in-tank)   │   868MHz    │              │                    │
│  │  pH/Temp/NH3 │   mesh     │              │                    │
│  │  NO2/NO3/DO  │            │              │                    │
│  │  TDS/Turbid  │            │   HUB NODE   │                    │
│  └──────────────┘            │  (RP2040 +   │──── WiFi6 ────► Cloud
│                              │   ESP32-C6)  │                  Dashboard
│  ┌──────────────┐            │              │                    + ML
│  │  FEEDER NODE │◄──────────►│              │                    Pipeline
│  │  (on-rim)    │   Sub-GHz  │              │                    + Alerts
│  │  Peristaltic │   mesh     │              │
│  │  pumps ×6   │            │              │─── BLE ──────► Mobile App
│  │  Servo feed  │            │              │                  (React Native)
│  │  RGBW LED    │            │              │
│  └──────────────┘            └──────┬───────┘
│  ┌──────────────┐                   │ Sub-GHz mesh
│  │ SENSOR NODE 2◄───────────────────┘
│  │ (add'l tank) │  (up to 8 sensor nodes
│  │ or sump)     │   per hub)
│  └──────────────┘
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                    CLOUD / EDGE SOFTWARE                     │  │
│  │  ┌─────────┐  ┌──────────────┐  ┌───────────────────────┐   │  │
│  │  │Dashboard│  │ ML Pipeline  │  │ Mobile App            │   │  │
│  │  │ (React) │  │ (TF/PyTorch)│  │ (React Native)        │   │  │
│  │  │ Realtime│  │ Anomaly det │  │ Push alerts           │   │  │
│  │  │ History │  │ Dose calc   │  │ Feed from camera      │   │  │
│  │  │ Config  │  │ Species DB  │  │ Tank config wizard     │   │  │
│  │  └─────────┘  └──────────────┘  └───────────────────────┘   │  │
│  └──────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the Sub-GHz mesh to WiFi/BLE/cloud.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + sensors, ESP32-C6 handles WiFi/BLE |
| Radio | SX1262 (868MHz) | Sub-GHz LoRa mesh to all nodes |
| Display | 2.8" IPS TFT (ILI9341) | Local status display |
| Storage | 32MB Flash + SD card | Data logging, OTA updates |
| Audio | Piezo buzzer + MAX9814 | Local alarms + voice status |
| Power | 5V USB-C + Lipo backup | Stays running during power outage |
| Connectors | 4× I2C, 2× UART, 8× GPIO | Expansion for more sensors |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler for all nodes)
- Data aggregation and time-series buffering
- WiFi uplink to MQTT broker (QoS 1, TLS)
- BLE GATT server for mobile app
- TFT dashboard rendering
- Local alarm triggers (buzzer + display)
- OTA update distribution to all nodes

### 2. Sensor Node (1-8 per system)

Submerged in-tank water quality monitor. Fully potted in marine-grade epoxy.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32L476RG | Ultra-low-power ARM Cortex-M4 |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| pH Sensor | EZO-pH (Atlas Scientific) | ±0.01 pH accuracy |
| Temp | DS18B20 (waterproof) | ±0.1°C accuracy |
| Ammonia | EZO-NH3 (Atlas Scientific) | Free ammonia NH3 |
| Nitrite | EZO-NO2 (Atlas Scientific) | NO2-N |
| Nitrate | EZO-NO3 (Atlas Scientific) | NO3-N |
| Dissolved O2 | EZO-DO (Atlas Scientific) | mg/L DO |
| TDS | EZO-EC (Atlas Scientific) | µS/cm conductivity |
| Turbidity | TSZ-01 (analog) | NTU turbidity |
| Power | 3.7V Lipo 800mAh + Qi wireless charging | Submerged, charges wirelessly |
| Depth Rating | IP68, 3m | Fully submersible |

**Sensor node firmware:**
- Reads all 8 water quality parameters every 60 seconds
- On-board calibration storage (NVS)
- Mesh TDMA time-slot transmission to hub
- Ultra-low-power sleep between readings (~50µA avg)
- Self-diagnosis: detects sensor drift, fouling, disconnection
- Wireless charging controller (Qi receiver coil)

### 3. Feeder Node (1 per system)

Rim-mounted actuator unit. Does the actual work of dosing and feeding.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 | Handles camera + pumps + LED |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Camera | OV2640 (2MP) | Fish behavior observation |
| Pumps | 6× peristaltic (Kamoer NKP) | Dose chemicals precisely |
| Feeder | Servo-driven hopper | Portion-controlled feeding |
| LED | 4× 50W RGBW LED arrays | Full-spectrum tank lighting |
| LED Driver | 4× AL8860 (PWM dimmer) | Per-channel 0-100% dimming |
| Power | 24V DC barrel jack (15W) | Powers pumps + LEDs |
| Flow Sensors | 6× YF-S201 | Verify pump output |

**Feeder node firmware:**
- Receives dosing commands from hub (volume + channel)
- Runs peristaltic pumps with flow verification
- Feeds precise portions on schedule
- Drives 4× RGBW LED channels with circadian schedule
- Camera streams to hub for behavior analysis
- Reports pump/flow anomalies to hub

### 4. Cloud Dashboard

React.js web app + Python ML backend.

- Real-time sensor data with 24h/7d/30d history graphs
- Water parameter trend analysis with prediction
- Dosing history and chemical inventory tracking
- Fish species database with care requirements
- Alert configuration (per-parameter thresholds)
- OTA firmware update management
- Multi-tank support (one hub per tank)

---

## Communication Protocol

### Sub-GHz Mesh (SX1262/61, 868MHz LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa SF7 (normal) / SF10 (long range) |
| Bandwidth | 125 kHz |
| TX Power | +14 dBm (EU) / +20 dBm (US) |
| Range | 30m indoor (normal) / 200m (long range) |
| Protocol | Custom TDMA (hub is coordinator) |
| Slot Duration | 100ms per node |
| Cycle Time | 1 second (8 slots + 2 control) |

### TDMA Frame Structure

```
| SLOT 0 (HUB) | SLOT 1 (SENSOR1) | SLOT 2 (SENSOR2) | ... | SLOT 7 (SENSOR7) | SLOT 8 (FEEDER) | SLOT 9 (CTRL) |
|   100ms      |     100ms        |     100ms        |     |     100ms        |     100ms        |    100ms      |

Total frame: 1 second
Slot 0: Hub broadcasts sync + commands
Slots 1-7: Sensor nodes uplink data
Slot 8: Feeder node uplink status + camera trigger
Slot 9: Control/ACK/retransmit
```

### Mesh Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2) ]

TYPE values:
  0x01 = SENSOR_DATA (8 floats: pH, temp, NH3, NO2, NO3, DO, TDS, turbidity)
  0x02 = FEEDER_STATUS (pump states, flow, hopper level, LED state)
  0x03 = COMMAND (dose/feed/light/alarm)
  0x04 = ACK
  0x05 = OTA_BLOCK (firmware update chunk)
  0x06 = CALIBRATION (sensor calibration data)
  0x07 = ALARM (critical alert)
  0x08 = HEARTBEAT
```

---

## AI / ML Pipeline

### 1. Anomaly Detection (on-hub, TFLite Micro)

- Input: Rolling window of 12 water parameters (last 60 readings = 1 hour)
- Model: 1D-CNN + LSTM hybrid, INT8 quantized, 120KB
- Output: Anomaly score (0-1) per parameter + overall
- Triggers: Score >0.7 = warning, >0.9 = critical alarm
- Detects: pH crash, ammonia spike, temperature drift, nitrite/nitrate buildup, oxygen depletion, turbidity events

### 2. Dose Calculator (on-hub, rule-based + ML refinement)

- Rule engine: Maps parameter deviations to dosing amounts using species-specific lookup tables
- ML refinement: Adjusts doses based on tank's historical response (how much buffer raised pH last time)
- Tracks: Chemical inventory, auto-reorders when low

### 3. Fish Behavior Analysis (cloud, PyTorch)

- Input: Camera frames from feeder node (OV2640, 1 fps)
- Model: YOLOv8-nano for fish detection + tracking
- Behaviors detected: Feeding activity, hiding, gasping at surface, erratic swimming, spawning
- Feeds into: Dynamic feeding schedule (if fish aren't eating, reduce food), health alerts

### 4. Circadian Lighting Optimizer (on-hub)

- Generates natural dawn/dusk/spectrum schedules based on:
  - Fish species (tropical vs temperate)
  - Plant species (low/high light)
  - Season (adjusts photoperiod)
  - Algae suppression (reduces blue during algae risk periods)
- Output: 4-channel PWM schedule (R/G/B/W) with 1-minute resolution

---

## Pin Assignments

### Hub Node (RP2040 + ESP32-C6)

**RP2040 (mesh coordinator + local I/O):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | UART0 TX/RX | ESP32-C6 UART2 (inter-MCU link) |
| GPIO4/GPIO5 | I2C0 SDA/SCL | SX1262 (Sub-GHz radio) |
| GPIO6 | SPI0 SCK | SD card + TFT |
| GPIO7 | SPI0 MOSI | SD card + TFT |
| GPIO8 | SPI0 MISO | SD card + TFT |
| GPIO9 | SPI0 CS0 | SD card CS |
| GPIO10 | SPI0 CS1 | TFT CS |
| GPIO11 | TFT DC | Display data/command |
| GPIO12 | TFT RESET | Display reset |
| GPIO13 | TFT BACKLIGHT | Display backlight PWM |
| GPIO14 | SX1262 BUSY | Radio busy signal |
| GPIO15 | SX1262 IRQ | Radio interrupt |
| GPIO16 | SX1262 NRST | Radio reset |
| GPIO17 | SX1262 NSS | Radio SPI chip select |
| GPIO18-21 | SPI1 | SX1262 SPI bus |
| GPIO22 | PIEZO | Buzzer PWM output |
| GPIO23 | USER_BTN | Front panel button |
| GPIO24 | LED_R | Status LED red |
| GPIO25 | LED_G | Status LED green |
| GPIO26 | LED_B | Status LED blue |

**ESP32-C6 (WiFi/BLE bridge):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | I2C SDA/SCL | (expansion port) |
| GPIO2/GPIO3 | UART0 TX/RX | Debug console |
| GPIO4/GPIO5 | UART1 TX/RX | RP2040 UART0 |
| GPIO12/GPIO13 | USB D+/D- | USB-C port |
| GPIO6-11 | SPI | Flash (internal) |

### Sensor Node (STM32L476RG)

| Pin | Function | Connected To |
|-----|----------|-------------|
| PA9/PA10 | UART1 TX/RX | EZO-pH carrier |
| PA2/PA3 | UART2 TX/RX | EZO-DO carrier |
| PB10/PB11 | UART3 TX/RX | EZO-EC carrier |
| PC4/PC5 | UART4 TX/RX | EZO-NH3 carrier |
| PD5/PD6 | UART5 TX/RX | EZO-NO2 carrier |
| PE0/PE1 | UART6 TX/RX | EZO-NO3 carrier |
| PA4 | ADC1_CH4 | Turbidity sensor (analog) |
| PB6/PB7 | I2C1 SDA/SCL | SX1261 radio |
| PA8 | ONE_WIRE | DS18B20 temperature |
| PC6 | QI_CHG | Qi wireless charge status |
| PB0 | VBAT_SENSE | Battery voltage ADC |
| PA0 | CHG_EN | Charge enable control |

### Feeder Node (ESP32-S3)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | I2C SDA/SCL | SX1261 radio |
| GPIO2-7 | SPI2 | OV2640 camera |
| GPIO8 | CAM_PWDN | Camera power down |
| GPIO9 | CAM_RESET | Camera reset |
| GPIO10 | CAM_XCLK | Camera clock (20MHz) |
| GPIO11-16 | CAM_DATA | Camera parallel data |
| GPIO17 | SERVO_PWM | Feeder hopper servo |
| GPIO18 | PUMP1_PWM | Peristaltic pump 1 (dechlorinator) |
| GPIO19 | PUMP2_PWM | Peristaltic pump 2 (pH buffer) |
| GPIO20 | PUMP3_PWM | Peristaltic pump 3 (fertilizer) |
| GPIO21 | PUMP4_PWM | Peristaltic pump 4 (medication A) |
| GPIO26 | PUMP5_PWM | Peristaltic pump 5 (medication B) |
| GPIO27 | PUMP6_PWM | Peristaltic pump 6 (buffer/calibration) |
| GPIO28-31 | FLOW1-4 | Flow sensor inputs (YF-S201) |
| GPIO32-35 | LED_PWM_R/G/B/W | AL8860 LED driver dimming (4ch) |
| GPIO36 | HOPPER_IR | Infrared hopper level sensor |
| GPIO37 | TEMP_NTC | NTC thermistor (pump compartment) |
| GPIO38 | FAN_PWM | Cooling fan control |
| GPIO39 | PWR_SENSE | 24V supply monitor |

---

## Power Architecture

### Hub Node
```
USB-C 5V ──► MCP73831 ──► Lipo 2000mAh ──► AP2112-3.3V ──► RP2040 + ESP32-C6
                                        ──► AP6212-1.8V ──► SX1262
                           TFT backlight: 5V direct via MOSFET
```
- Average draw: 180mA (WiFi on) → ~11 hours on battery
- Battery backup: auto-fails to battery on USB loss, mesh keeps running

### Sensor Node
```
Qi Receiver (5V) ──► MCP73831 ──► Lipo 800mAh ──► AP2112-3.3V ──► STM32L476
                                                             ──► TLV702-1.8V ──► SX1261
                                         EZO sensors: 3.3V (shared rail, decoupled)
```
- Average draw: 2.5mA (1 reading/min + 1 TX/sec) → ~13 days on battery
- Qi charging pad sits under/behind tank

### Feeder Node
```
24V DC barrel ──► LM2596-5V ──► AP2112-3.3V ──► ESP32-S3 + radio
                        ──► AL8860×4 ──► RGBW LEDs (50W each, 24V)
                        ──► DRV8833×3 ──► Peristaltic pumps (6× 5V)
                        ──► Servo (5V direct)
```
- Average draw: 5W (LEDs at 25%) to 60W (LEDs at 100% + pumps)
- 24V 3A supply recommended

---

## Mechanical Design

### Hub Node
- Enclosure: 120×80×30mm ABS plastic (3D printed or injection)
- Wall-mountable (keyhole slots) or desktop
- TFT visible through front window
- Piezo speaker port on side
- USB-C port on bottom
- External SMA antenna connector for Sub-GHz

### Sensor Node
- Probe form factor: 25mm diameter × 180mm long
- Marine-grade epoxy potting (3M DP270)
- Titanium electrode guards (corrosion-proof)
- DS18B20 probe extends 50mm below main body
- Qi receiver coil in top cap (above water line)
- Neodymium magnet mount (sticks to tank wall)
- Depth: fully submersible, rated to 3m

### Feeder Node
- Rim-mounted bracket (adjustable, fits 6-19mm glass)
- Main body: 200×80×60mm
- Hopper: 150mL capacity (≈2 weeks of flake food)
- 6 pump cartridge slots (snap-in peristaltic pump modules)
- LED array: 4× 50W panels on adjustable gooseneck arms
- Camera: top-down view through glass, IR illumination for night
- Fan-cooled pump compartment

---

## Full BOM

### Hub Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | RP2040 | QFN-56 7x7 | 1 | $1.20 | $1.20 |
| 2 | ESP32-C6-MINI-1 | Module | 1 | $3.20 | $3.20 |
| 3 | SX1262 | QFN-24 | 1 | $4.50 | $4.50 |
| 4 | 2.8" IPS TFT (ILI9341) | Module | 1 | $6.80 | $6.80 |
| 5 | 32MB W25Q256 | SOIC-8 | 1 | $1.80 | $1.80 |
| 6 | SD card slot | Micro push-push | 1 | $0.50 | $0.50 |
| 7 | MCP73831 | SOT-23-5 | 1 | $0.40 | $0.40 |
| 8 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 9 | AP6212-1.8 | SOT-23-5 | 1 | $0.35 | $0.35 |
| 10 | Lipo 2000mAh | Custom | 1 | $4.50 | $4.50 |
| 11 | USB-C receptacle | 16-pin SMD | 1 | $0.35 | $0.35 |
| 12 | SMA connector | Edge-mount | 1 | $0.80 | $0.80 |
| 13 | Antenna 868MHz | Wire/PCB | 1 | $1.50 | $1.50 |
| 14 | Piezo buzzer | 12mm SMD | 1 | $0.40 | $0.40 |
| 15 | Passives (R/C/L/inductors) | 0402 | ~60 | $1.50 | $1.50 |
| 16 | PCB 4-layer | 120×80mm | 1 | $3.00 | $3.00 |
| | | | | **Subtotal** | **$31.10** |

### Sensor Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | STM32L476RG | LQFP-64 | 1 | $5.80 | $5.80 |
| 2 | SX1261 | QFN-24 | 1 | $3.80 | $3.80 |
| 3 | EZO-pH carrier | DIP-6 | 1 | $28.00 | $28.00 |
| 4 | EZO-DO carrier | DIP-6 | 1 | $34.00 | $34.00 |
| 5 | EZO-EC carrier | DIP-6 | 1 | $30.00 | $30.00 |
| 6 | EZO-NH3 carrier | DIP-6 | 1 | $42.00 | $42.00 |
| 7 | EZO-NO2 carrier | DIP-6 | 1 | $38.00 | $38.00 |
| 8 | EZO-NO3 carrier | DIP-6 | 1 | $38.00 | $38.00 |
| 9 | DS18B20 waterproof | Probe | 1 | $3.50 | $3.50 |
| 10 | Turbidity sensor | Analog module | 1 | $4.00 | $4.00 |
| 11 | Qi receiver | Module 5V 1A | 1 | $3.20 | $3.20 |
| 12 | MCP73831 | SOT-23-5 | 1 | $0.40 | $0.40 |
| 13 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 14 | Lipo 800mAh | Custom pouch | 1 | $3.00 | $3.00 |
| 15 | Antenna 868MHz | Wire/PCB | 1 | $1.50 | $1.50 |
| 16 | Epoxy potting (3M DP270) | 50mL | 1 | $12.00 | $12.00 |
| 17 | Titanium electrode guards | Custom | 1 | $5.00 | $5.00 |
| 18 | Neodymium magnet mount | Assembly | 1 | $2.00 | $2.00 |
| 19 | Passives | 0402 | ~40 | $1.00 | $1.00 |
| 20 | PCB 4-layer (round 25mm + long strip) | Custom | 1 | $4.00 | $4.00 |
| | | | | **Subtotal** | **$261.70** |

### Feeder Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | ESP32-S3-WROOM-1 | Module | 1 | $3.50 | $3.50 |
| 2 | SX1261 | QFN-24 | 1 | $3.80 | $3.80 |
| 3 | OV2640 camera | Module | 1 | $2.50 | $2.50 |
| 4 | Kamoer NKP peristaltic pump | Module | 6 | $8.00 | $48.00 |
| 5 | DRV8833 motor driver | HTSSOP-16 | 3 | $1.80 | $5.40 |
| 6 | MG996R servo | Standard | 1 | $3.50 | $3.50 |
| 7 | 50W RGBW LED array | Module | 4 | $8.00 | $32.00 |
| 8 | AL8860 LED driver | SOT-89 | 4 | $1.20 | $4.80 |
| 9 | LM2596-5V buck | TO-263 | 1 | $1.50 | $1.50 |
| 10 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 11 | YF-S201 flow sensor | Module | 6 | $2.50 | $15.00 |
| 12 | IR hopper level sensor | Module | 1 | $1.00 | $1.00 |
| 13 | NTC 10K thermistor | 0402 | 1 | $0.10 | $0.10 |
| 14 | 40mm fan | 5V DC | 1 | $1.50 | $1.50 |
| 15 | DC barrel jack | 5.5×2.1mm | 1 | $0.30 | $0.30 |
| 16 | Antenna 868MHz | Wire/PCB | 1 | $1.50 | $1.50 |
| 17 | Passives (R/C/L/inductors) | 0402 | ~80 | $2.00 | $2.00 |
| 18 | PCB 4-layer | 200×80mm | 1 | $4.00 | $4.00 |
| 19 | Hopper (3D printed) | PETG | 1 | $2.00 | $2.00 |
| 20 | Rim mount bracket | 3D printed | 1 | $2.00 | $2.00 |
| 21 | Gooseneck LED arms (4) | Custom | 4 | $3.00 | $12.00 |
| 22 | 24V 3A power supply | Desktop | 1 | $8.00 | $8.00 |
| | | | | **Subtotal** | **$155.20** |

### System Total (1 hub + 1 sensor + 1 feeder)

**Hardware BOM: ~$448.00**

---

## Software Stack

### Cloud Dashboard (React + FastAPI)

```
software/dashboard/
├── frontend/              # React + Vite + TailwindCSS
│   ├── src/
│   │   ├── components/    # Sensor cards, charts, alerts
│   │   ├── hooks/         # Real-time MQTT subscription
│   │   ├── pages/         # Dashboard, History, Config, Species
│   │   └── App.tsx
│   └── package.json
├── backend/               # FastAPI (Python)
│   ├── main.py            # REST + WebSocket server
│   ├── models.py          # SQLAlchemy tank/sensor models
│   ├── mqtt_bridge.py     # MQTT → DB + WebSocket relay
│   ├── dosing_engine.py   # Rule-based dose calculation
│   └── requirements.txt
└── docker-compose.yml     # Postgres + Mosquitto + API + Frontend
```

### ML Pipeline (Python)

```
software/ml-pipeline/
├── train_anomaly.py       # Train 1D-CNN+LSTM anomaly detector
├── train_behavior.py      # Train YOLOv8-nano fish detector
├── export_tflite.py       # Convert → TFLite INT8 for hub
├── inference_server.py    # Cloud inference for camera frames
├── datasets/              # Training data format specs
└── requirements.txt
```

### Mobile App (React Native)

```
software/mobile-app/
├── App.tsx                # Navigation: Home, Tank, Alerts, Settings
├── screens/
│   ├── TankOverview.tsx   # Real-time parameter display
│   ├── AlertHistory.tsx   # Push notification history
│   ├── FeedLog.tsx        # Feeding + dosing log
│   ├── CameraView.tsx     # Live camera stream from feeder
│   └── SetupWizard.tsx    # First-time tank configuration
├── services/
│   ├── ble.ts             # Direct BLE connection to hub
│   ├── mqtt.ts            # Cloud MQTT subscription
│   └── push.ts            # FCM/APNs push notification
└── package.json
```

---

## Aquarium Species Database

Built into the cloud backend, accessible from dashboard and mobile app:

| Parameter | Tropical Freshwater | Marine Reef | Coldwater |
|-----------|--------------------|----|-----------|
| pH range | 6.5-7.5 | 8.1-8.4 | 7.0-7.8 |
| Temp °C | 24-28 | 25-27 | 15-20 |
| NH3 ppm | <0.25 | <0.02 | <0.25 |
| NO2 ppm | <0.25 | <0.02 | <0.25 |
| NO3 ppm | <40 | <5 | <40 |
| DO mg/L | >5 | >6 | >6 |
| TDS ppm | 150-300 | 30000-35000 | 150-300 |
| Light hrs | 8-10 | 10-12 | 6-8 |

Each species entry includes: safe ranges, dosing recommendations, compatibility, feeding preferences, common diseases, and behavioral indicators.

---

## Alert System

| Level | Condition | Action |
|-------|-----------|--------|
| INFO | Parameter trending slightly out of range | Dashboard notification |
| WARNING | Parameter 20% outside safe range | Push notification + buzzer beep |
| CRITICAL | Parameter 40%+ outside safe range OR ammonia/NO2 spike | Push + SMS + buzzer alarm + auto-dose buffer |
| EMERGENCY | Life-threatening (NH3 >1ppm, DO <2mg/L, temp >35°C) | Push + SMS + email + continuous alarm + auto-shutdown heater/emergency dose |

---

## Getting Started

### Hardware Assembly
See `docs/assembly_guide.md` for detailed step-by-step instructions for each node.

### Flash Firmware
```bash
# Hub node (RP2040)
cd firmware/hub-node
mkdir build && cd build
cmake .. -DPICO_BOARD=pico
make -j4
# Flash via USB: hold BOOTSEL, copy .uf2 to RPI-RP2 drive

# Hub node (ESP32-C6)
cd firmware/hub-node/esp32
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyUSB0 flash

# Sensor node (STM32)
cd firmware/sensor-node
mkdir build && cd build
cmake .. -DTARGET=stm32l476rg
make -j4
# Flash via ST-Link: st-flash write aqua_guard_sensor.bin 0x08000000

# Feeder node (ESP32-S3)
cd firmware/feeder-node
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB1 flash
```

### Cloud Dashboard
```bash
cd software/dashboard
docker-compose up -d
# Access at http://localhost:3000
```

### Mobile App
```bash
cd software/mobile-app
npm install
npx react-native run-android  # or run-ios
```

---

## Directory Structure

```
aqua-guard/
├── README.md
├── schematic/
│   ├── hub-node/           # KiCad project for hub
│   ├── sensor-node/        # KiCad project for sensor probe
│   └── feeder-node/        # KiCad project for feeder
├── firmware/
│   ├── hub-node/           # RP2040 + ESP32-C6 firmware
│   ├── sensor-node/        # STM32L476 firmware
│   ├── feeder-node/        # ESP32-S3 firmware
│   └── common/             # Shared mesh protocol, CRC, packet defs
├── hardware/
│   ├── bom/                # BOM.csv per node
│   ├── enclosure/          # 3D-printable STEP/STL files
│   └── gerbers/            # Production gerber files
├── software/
│   ├── dashboard/          # React + FastAPI web app
│   ├── ml-pipeline/        # Training scripts for anomaly + behavior models
│   └── mobile-app/         # React Native mobile app
├── scripts/                # Calibration, deployment, OTA scripts
└── docs/                   # Assembly, API, protocol, architecture docs
```

---

*Invented 2026-06-12 by jayis1*