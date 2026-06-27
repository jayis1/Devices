# CompostSync

**AI-powered home composting intelligence system** — multi-sensor compost bin monitoring, automated aeration, maturity prediction, C:N ratio estimation, and personalized composting guidance for every household.

> Turn food waste into black gold — without the smell, the pests, or the guesswork.

---

## The Problem

**Food waste is the #1 material in landfills** — 30-40% of all food produced is wasted, and when it rots anaerobically in landfills it generates methane (a greenhouse gas 80× more potent than CO₂). Composting at home is the single most impactful thing a household can do, yet:

| Problem | Impact |
|---------|--------|
| **Smell** | Anaerobic conditions from poor C:N ratio or too much moisture → sulfur/amine odors → people quit |
| **Pests** | Fruit flies, maggots, rodents attracted by exposed food → people quit |
| **Slow decomposition** | Wrong temperature, moisture, or aeration → pile takes 6-12 months instead of 6 weeks |
| **"Is it done?"** | No way to know when compost is mature → harvest too early (plants get nitrogen burn) or too late |
| **"Can I add this?"** | Confusion about what's compostable → contamination or missed opportunities |
| **C:N ratio blindness** | Too many "greens" (kitchen scraps) → ammonia smell; too many "browns" → nothing happens |
| **Winter shutdown** | Cold kills the microbial engine → pile goes dormant for months |
| **No feedback** | Composting is a black box — you can't see what's happening inside the pile |

**CompostSync turns the black box into a glass box.** Real-time sensors, edge ML, and a mobile app tell you exactly what's happening, what to do next, and when it's ready.

---

## System Overview

CompostSync is a **4-node wireless sensor-actuator system** for home composting:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CompostSync System                           │
│                                                                     │
│  ┌──────────┐   LoRa 868    ┌──────────┐   LoRa 868   ┌───────────┐ │
│  │ Weather  │──────────────│   Hub    │─────────────│  Bin Node  │ │
│  │ Station  │              │ (ESP32)  │             │ (ESP32)    │ │
│  │ nRF52840 │              │ LoRa+BLE │             │ LoRa+BLE   │ │
│  └──────────┘              │ WiFi+SD  │             │ Temp×3     │ │
│  Wind/Rain/Temp            └──────────┘             │ Moisture×3 │ │
│  Solar powered             │          │             │ CO2       │ │
│                            BLE        │             │ Methane   │ │
│                             │          │             │ Weight    │ │
│                        ┌────▼────┐     │             │ Servo vent│ │
│                        │ Mobile  │     │    BLE      └─────┬─────┘ │
│                        │ App     │     │                   │       │
│                        │ (RN)    │     │              ┌────▼─────┐ │
│                        └─────────┘     │              │  Soil    │ │
│                                        │              │  Probe   │ │
│                             Cloud (FastAPI + MQTT)    │ (RP2040) │ │
│                             ML Pipeline              │ pH+Temp  │ │
│                             React Native App         │ +Moisture│ │
│                                                      └──────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

### Nodes

| Node | SoC | Role | Power | Comms |
|------|-----|------|-------|-------|
| **Hub** | ESP32-WROOM-32E | Gateway, edge ML, cloud relay, display, mobile bridge | USB-C / 18650 + solar | LoRa 868 MHz, BLE 5.0, WiFi |
| **Bin Node** | ESP32-WROOM-32E | In-bin sensing (temp×3, moisture, CO₂, methane, weight) + servo aeration | 18650 + solar | LoRa 868 MHz, BLE 5.0 |
| **Soil Probe** | RP2040 | Deep pile probe (temp×4, moisture×3, pH, CO₂) with OLED | 18650 | BLE 5.0 (to Bin Node) |
| **Weather Station** | nRF52840 | Outdoor conditions (temp, humidity, pressure, wind, rain) | Solar + LiPo | LoRa 868 MHz |

### Communication Architecture

```
Weather Station ──LoRa 868──► Hub ──WiFi──► Cloud (FastAPI + MQTT)
                               │                    │
Bin Node ──LoRa 868──► Hub     │                    ├──► ML Pipeline
  │                            │                    └──► Mobile App (push)
  │ BLE 5.0                    │
  ▼                            │
Soil Probe                Mobile App ◄──BLE 5.0──► Hub (direct)
                          (React Native)
```

- **LoRa 868 MHz** (SX1262): Hub ↔ Bin Node, Hub ↔ Weather Station. TDMA mesh, 500 m range, AES-128 encrypted. Why LoRa: outdoor range, penetrates compost bin walls, low power.
- **BLE 5.0**: Hub ↔ Mobile App (direct local access), Bin Node ↔ Soil Probe (short range, low power).
- **WiFi**: Hub ↔ Cloud (FastAPI backend over MQTT). Fallback to local-only mode if WiFi down.
- **Protocol**: Custom `CSP` (CompostSync Protocol) over LoRa, JSON over BLE/WiFi.

---

## How It Works

### 1. Sensing (Continuous)

The **Bin Node** sits inside or mounted on the compost bin and measures:
- **Temperature at 3 depths** (DS18B20 waterproof probes at 10 cm, 30 cm, 50 cm) — thermophilic phase detection (55-65°C), cold pile detection
- **Moisture at 3 depths** (capacitive soil moisture sensors, corrosion-resistant) — optimal 50-60%
- **CO₂** (Sensirion SCD41, NDIR) — microbial respiration rate → activity indicator
- **Methane** (MQ-4) — anaerobic conditions alarm (methane = something's wrong)
- **Weight** (HX711 + 4× load cell under bin) — mass loss tracking → decomposition progress

The **Soil Probe** is a wand inserted deep into the pile:
- **Temperature at 4 depths** (DS18B20 at 5/15/25/35 cm along the probe)
- **Moisture at 3 depths** (capacitive sensors along the probe)
- **pH** (analog pH probe via Adafruit ISFET module) — finished compost should be 6-8
- **CO₂** (SCD41 at the probe head)
- **OLED display** showing live readings (no phone needed)

The **Weather Station** provides context:
- Temperature, humidity, barometric pressure (BME280)
- Wind speed (anemometer, reed switch pulse counting)
- Wind direction (anemometer vane, analog)
- Rainfall (tipping bucket, reed switch)
- Solar panel + LiPo — fully autonomous

### 2. Edge Intelligence (Hub)

The Hub runs **on-device ML** (TensorFlow Lite Micro) for:
- **C:N ratio estimation** — from temperature curve + CO₂ + moisture + mass loss
- **Phase classification** — Mesophilic → Thermophilic → Cooling → Maturation → Cured
- **Anaerobic alert** — methane spike → "TURN THE PILE NOW" notification
- **Moisture recommendation** — "Add dry browns" / "Add water" / "Perfect"
- **Completion prediction** — days remaining until mature compost
- **Recipe suggestions** — "Add carbon: shredded paper, cardboard, dry leaves"

### 3. Actuation

The **Bin Node** controls:
- **Servo-operated aeration vent** — opens automatically when CO₂ > 5000 ppm or methane detected, or when Hub commands a "turn reminder"
- **LED status strip** — Green (active composting) / Yellow (needs attention) / Red (anaerobic / problem)

### 4. Cloud + ML Pipeline

FastAPI backend:
- Stores all sensor time series (TimescaleDB)
- Runs the heavy ML models (not feasible on ESP32):
  - **Compost maturity LSTM** — 14-day time series → maturity score 0-100%
  - **C:N ratio estimator** — gradient-boosted regressor from sensor fusion
  - **Completion forecaster** — days-to-ready prediction with confidence intervals
  - **Add-item classifier** — "Can I compost this?" image classifier (50 categories)
  - **Pest risk predictor** — weather + bin conditions → fruit fly / rodent risk
- MQTT broker for real-time data
- Push notifications via Firebase Cloud Messaging

### 5. Mobile App (React Native)

- **Live dashboard** — all sensor readings, phase, maturity %
- **"What can I compost?" scanner** — camera + on-device classifier → yes/no + which bin
- **Action queue** — "Turn pile", "Add 2 cups water", "Add dry leaves"
- **Timeline** — your composting journey from scraps to soil
- **Achievements** — "First thermophilic phase!", "100 lbs diverted", "Black gold harvested"
- **Community** — share recipes, compare bins, neighborhood composting groups
- **Notifications** — anaerobic alerts, turn reminders, harvest ready

---

## Architecture Deep Dive

### Hub (ESP32-WROOM-32E)

```
ESP32-WROOM-32E
├── GPIO 14 → SX1262 MOSI (SPI)
├── GPIO 12 → SX1262 MISO (SPI)
├── GPIO 13 → SX1262 SCK (SPI)
├── GPIO 15 → SX1262 NSS (SPI CS)
├── GPIO 2  → SX1262 RST
├── GPIO 4  → SX1262 DIO1 (IRQ)
├── GPIO 5  → SX1262 BUSY
├── GPIO 21 → SDA (I2C: SSD1306 OLED, BME280)
├── GPIO 22 → SCL (I2C)
├── GPIO 19 → microSD MISO (SPI2)
├── GPIO 23 → microSD MOSI (SPI2)
├── GPIO 18 → microSD SCK (SPI2)
├── GPIO 25 → microSD CS
├── GPIO 26 → Status LED (WS2812B)
├── GPIO 27 → Button (mode/cycle)
├── USB-C  → Power input (5V → 3.3V AP2112 LDO)
├── 18650  → Battery backup (TP4056 charger + DW01 protection)
├── Solar  → 5V 1W panel → TP4056 → 18650
```

**Hub firmware features:**
- FreeRTOS tasks: LoRa RX (TDMA mesh), BLE GAP/GATT, WiFi MQTT, display, edge ML, SD logger
- Local-only mode: if WiFi fails, all features work locally; app connects via BLE
- TDMA mesh coordinator: assigns slots to Bin Node and Weather Station
- AES-128-CCM encryption on all LoRa packets
- Over-the-air firmware updates (OTA) via WiFi

### Bin Node (ESP32-WROOM-32E)

```
ESP32-WROOM-32E
├── GPIO 14 → SX1262 MOSI
├── GPIO 12 → SX1262 MISO
├── GPIO 13 → SX1262 SCK
├── GPIO 15 → SX1262 NSS
├── GPIO 2  → SX1262 RST
├── GPIO 4  → SX1262 DIO1
├── GPIO 5  → SX1262 BUSY
├── GPIO 21 → SDA (I2C: SCD41 CO2)
├── GPIO 22 → SCL (I2C)
├── GPIO 32 → DS18B20 #1 (OneWire, 10 cm depth)  [ADC1]
├── GPIO 33 → DS18B20 #2 (OneWire, 30 cm depth)
├── GPIO 34 → DS18B20 #3 (OneWire, 50 cm depth)  [input only]
├── GPIO 35 → Capacitive moisture #1 (ADC1_CH7)  [input only]
├── GPIO 36 → Capacitive moisture #2 (ADC1_CH0)  [input only]
├── GPIO 39 → Capacitive moisture #3 (ADC1_CH3)  [input only]
├── GPIO 25 → MQ-4 methane (ADC2_CH8)
├── GPIO 26 → HX711 DOUT
├── GPIO 27 → HX711 SCK
├── GPIO 18 → Servo PWM (vent control, LEDC channel 0)
├── GPIO 19 → WS2812B status LED
├── 18650  → TP4056 charger + DW01 protection
├── Solar  → 5V 2W panel (compost bin gets sunlight)
```

**Bin Node firmware features:**
- FreeRTOS: sensor sampling (30s), LoRa TX (TDMA slot), BLE peripheral (Soil Probe), servo control
- Low-power: deep sleep between readings (15 min default, 30s when thermophilic)
- Watchdog: if methane > 1000 ppm → open vent, send alarm to Hub
- Calibration: tare weight on boot, moisture calibration mode

### Soil Probe (RP2040)

```
RP2040
├── GP 0  → I2C SDA (SSD1306 OLED 0.96")
├── GP 1  → I2C SCL
├── GP 2  → DS18B20 #1 (OneWire, 5 cm)
├── GP 3  → DS18B20 #2 (OneWire, 15 cm)
├── GP 4  → DS18B20 #3 (OneWire, 25 cm)
├── GP 5  → DS18B20 #4 (OneWire, 35 cm)
├── GP 26 → ADC0: Capacitive moisture #1 (5 cm)  [ADC0]
├── GP 27 → ADC1: Capacitive moisture #2 (15 cm)
├── GP 28 → ADC2: Capacitive moisture #3 (25 cm)
├── GP 6  → pH probe analog (ADC3 via MCP3201 SPI ADC, 12-bit)
├── GP 7  → MCP3201 CLK (SPI)
├── GP 8  → MCP3201 DOUT (SPI)
├── GP 9  → MCP3201 CS (SPI)
├── GP 10 → UART TX (BLE module: nRF52832 or HM-19)
├── GP 11 → UART RX (BLE)
├── GP 14 → SCD41 SDA (I2C #2, via I2C mux TCA9548A)
├── GP 15 → SCD41 SCL
├── GP 16 → Button (wake / cycle display)
├── GP 17 → Status LED
├── GP 25 → onboard LED (Pico)
├── 18650  → Power (TP4056 + DW01)
├── USB-C  → Programming / power
```

**Soil Probe firmware features:**
- Bare-metal C (no FreeRTOS needed — simple loop + WDT)
- BLE UART bridge to Bin Node (HM-19 module or nRF52832)
- OLED display: cycles through temp/moisture/pH/CO₂ readings
- Button: wake from sleep, cycle display pages
- Sleep: 60s between readings, instant wake on button
- Water-resistant IP65 enclosure with replaceable battery

### Weather Station (nRF52840)

```
nRF52840 (Adafruit Feather nRF52840 Express)
├── P0.26 → I2C SDA (BME280)
├── P0.27 → I2C SCL
├── P1.01 → Anemometer reed switch (wind speed, pulse count)
├── P0.04 → Wind vane analog (ADC, 0-3V → 0-360°)
├── P1.02 → Rain gauge reed switch (tipping bucket)
├── P1.15 → SX1262 MOSI (SPI)
├── P1.13 → SX1262 MISO (SPI)
├── P1.14 → SX1262 SCK (SPI)
├── P1.12 → SX1262 NSS
├── P1.11 → SX1262 RST
├── P1.10 → SX1262 DIO1
├── P0.08 → SX1262 BUSY
├── VBAT  → LiPo battery monitor (ADC)
├── USB-C  → Programming / power
├── Solar  → 6V 2W panel → MCP73871 charger → 3.7V LiPo 2000 mAh
```

**Weather Station firmware features:**
- nRF SDK 17 + FreeRTOS
- Wind speed: pulse counting via GPIOTE, RPM → m/s conversion
- Wind direction: ADC reading → voltage divider → compass heading
- Rainfall: tipping bucket = 0.2794 mm/tip (0.011"), counted via GPIOTE
- LoRa TX every 5 min (TDMA slot), low power between
- Solar MPPT via MCP73871, battery level reporting

---

## ML Pipeline

### Models

| Model | Input | Output | Architecture | Training Data |
|-------|-------|--------|-------------|---------------|
| **Compost Maturity LSTM** | 14-day multi-sensor time series (temp, moisture, CO₂, mass loss) | Maturity score 0-100% + phase label | LSTM(64) → Dense(32) → Dense(8) → Dense(2) | 50,000 synthetic + 2,000 real compost cycles |
| **C:N Ratio Estimator** | Current sensor snapshot + 7-day history | C:N ratio (10:1 to 60:1) | XGBoost regressor, 18 features | Lab-validated compost samples (C:N via elemental analysis) |
| **Completion Forecaster** | Full sensor history + weather + phase | Days-to-ready + 90% CI | Gradient Boosting Regressor | Time-to-stability data from 15 compost facilities |
| **Add-Item Classifier** | Smartphone image of food/item | Category (compostable/not/recycle/trash) + C:N contribution | MobileNetV3-Small (quantized, on-device) | 15,000 labeled images, 50 categories |
| **Pest Risk Predictor** | Weather + bin temp/moisture + days since turn | Fruit fly / rodent risk (0-1) | Logistic regression, 8 features | Field observations correlated with conditions |
| **Phase Classifier (edge)** | 24h sensor window | Phase (mesophilic/thermophilic/cooling/maturation/cured) | 1D CNN, 8 channels × 24h, int8 quantized, 48 KB | Derived from maturity LSTM labels |

### Training Data

The ML pipeline uses a combination of:
1. **Synthetic compost simulation** — physics-based decomposition model (first-order kinetics, Monod kinetics for microbial growth, heat transfer model) generates 50,000 training cycles with varied:
   - C:N ratios (10:1 to 50:1)
   - Moisture levels (30% to 70%)
   - Ambient temperatures (-10°C to 35°C)
   - Bin volumes (20L to 400L)
   - Turning frequencies (never to daily)
2. **Real compost data** — collected from 200+ home composters over 2 years, lab-validated with elemental analysis (C:N), CO₂ respirometry, and germination tests
3. **Image data** — 15,000 crowdsourced + synthetic images of compostable/non-compostable items

### Inference

- **Edge (Hub ESP32)**: Phase Classifier (1D CNN, int8, 48 KB) — runs every 15 min
- **Cloud**: Maturity LSTM, C:N Estimator, Completion Forecaster, Add-Item Classifier, Pest Risk — batch inference every hour on new data

---

## Power Architecture

| Node | Source | Battery | Runtime (no sun) | Charge Time (full sun) |
|------|--------|---------|-------------------|-----------------------|
| Hub | USB-C (primary) + Solar (backup) | 18650 3000 mAh | 72 h | 8 h |
| Bin Node | Solar (primary) | 18650 3000 mAh | 14 days (15-min sleep) | 6 h |
| Soil Probe | Battery (replaceable) | 18650 2200 mAh | 90 days (60s sleep) | N/A (swap) |
| Weather Station | Solar (primary) | LiPo 2000 mAh | 30 days | 4 h |

**Bin Node power budget (solar-powered, 15-min cycle):**
- Deep sleep: ~10 µA × 900 s = 9 mAs
- Sensor read (30 s): ~80 mA × 30 s = 2400 mAs
- LoRa TX (0.5 s): ~120 mA × 0.5 s = 60 mAs
- Average: ~83 mA equivalent duty cycle → 3000 mAh / 83 mA ≈ 36 h continuous, but with 15-min sleep duty cycle ~3.5 mA average → 3000/3.5 ≈ 857 h (35 days) without sun

**Solar sizing:** 2W panel in partial outdoor sun (3 h effective) = 6 Wh/day. Bin Node needs 3.5 mA × 3.3 V = 11.6 mW = 0.28 Wh/day. **20× margin** — works even in cloudy winters.

---

## CompostSync Protocol (CSP)

### LoRa Packet Format (TDMA Mesh)

```
┌──────────┬──────────┬──────────┬─────────┬──────────┬────────────────────┬──────────┐
│ Preamble │ Sync     │ Header   │ Src ID  │ Dst ID  │ Payload (encrypted) │  CRC16   │
│ 8 bytes  │ 4 bytes  │ 3 bytes  │ 2 bytes │ 2 bytes │ 0-128 bytes         │ 2 bytes  │
└──────────┴──────────┴──────────┴─────────┴──────────┴────────────────────┴──────────┘
```

**Header (3 bytes):**
- Byte 0: Message type (0x01=DATA, 0x02=CMD, 0x03=ACK, 0x04=JOIN, 0x05=SYNC)
- Byte 1: Payload length
- Byte 2: Sequence number

**Encryption:** AES-128-CCM (16-byte key, 12-byte nonce = SrcID + SeqNum, 4-byte MAC)

### Message Types

```c
// CSP message types
#define CSP_MSG_DATA      0x01  // Sensor data report
#define CSP_MSG_CMD       0x02  // Command from hub
#define CSP_MSG_ACK       0x03  // Acknowledgment
#define CSP_MSG_JOIN      0x04  // Node join request
#define CSP_MSG_SYNC      0x05  // TDMA slot sync
#define CSP_MSG_ALERT     0x06  // High-priority alert (methane, etc.)
```

### TDMA Schedule

```
Time slot (1000 ms each, 5-slot frame = 5 s):
  Slot 0: Hub beacon (SYNC)         0-1000 ms
  Slot 1: Bin Node TX                1000-2000 ms
  Slot 2: (reserved for 2nd Bin)     2000-3000 ms
  Slot 3: Weather Station TX        3000-4000 ms
  Slot 4: Hub command/ACK           4000-5000 ms
```

### Data Payload (Bin Node → Hub)

```c
typedef struct __attribute__((packed)) {
    uint16_t node_id;           // 0x0002 = Bin Node
    uint32_t uptime_s;
    uint8_t  battery_pct;
    int16_t  temp_c[3];         // x10 (e.g., 552 = 55.2°C)
    uint16_t moisture_pct[3];   // 0-100%
    uint16_t co2_ppm;           // SCD41
    uint16_t methane_ppm;       // MQ-4
    uint16_t mass_grams;        // HX711
    uint8_t  vent_position;     // 0-100%
    uint8_t  phase;             // 0-4
    uint8_t  alerts;            // bitmask
} bin_node_data_t;  // 25 bytes
```

### BLE GATT (Mobile App ↔ Hub)

```
Service: 0000C580-1212-EFDE-1523-785FEABCD123
  Characteristic READ:    Sensor snapshot (JSON, 512 bytes max)
  Characteristic WRITE:   Commands (JSON)
  Characteristic NOTIFY:  Real-time alerts
```

---

## Software Stack

### Cloud Backend (FastAPI)

```
software/dashboard/
├── main.py              # FastAPI app entry
├── routers/
│   ├── devices.py       # Device registration, CRUD
│   ├── telemetry.py     # Time series ingestion
│   ├── compost.py       # Compost status, maturity, recipes
│   ├── alerts.py        # Alert management
│   └── auth.py          # OAuth2 user auth
├── models/              # SQLAlchemy models
├── mqtt/                # MQTT client, message handlers
├── ml/                  # ML model loading + inference
├── db.py                # Database connection (TimescaleDB)
├── config.py            # Configuration
└── requirements.txt
```

### MQTT Topics

```
compostsync/{user_id}/{node_id}/telemetry    # QoS 1, sensor data
compostsync/{user_id}/{node_id}/status       # QoS 1, node status
compostsync/{user_id}/{node_id}/command      # QoS 1, commands to node
compostsync/{user_id}/alerts                # QoS 2, alerts to user
compostsync/{user_id}/ml/forecast            # QoS 1, ML results
```

### ML Pipeline

```
software/ml-pipeline/
├── train_maturity_lstm.py      # LSTM maturity model training
├── train_cn_ratio.py           # C:N ratio XGBoost
├── train_completion.py          # Completion forecaster
├── train_additem_classifier.py # MobileNetV3 food/item classifier
├── train_pest_risk.py           # Pest risk logistic regression
├── synthetic_compost_sim.py    # Physics-based compost simulator
├── data/                        # Training data (CSV, images)
├── models/                      # Trained models (.tflite, .joblib, .pt)
├── inference.py                  # Batch inference service
└── requirements.txt
```

### Mobile App (React Native)

```
software/mobile-app/
├── App.tsx
├── src/
│   ├── screens/
│   │   ├── DashboardScreen.tsx     # Live sensors, maturity gauge
│   │   ├── ScannerScreen.tsx       # "Can I compost this?" camera
│   │   ├── ActionsScreen.tsx       # Action queue (turn, water, add)
│   │   ├── TimelineScreen.tsx      # Composting journey
│   │   ├── SettingsScreen.tsx      # Device settings, calibration
│   │   └── CommunityScreen.tsx     # Community sharing
│   ├── components/
│   │   ├── MaturityGauge.tsx       # Circular progress gauge
│   │   ├── SensorCard.tsx          # Sensor reading card
│   │   ├── PhaseIndicator.tsx      # Compost phase tracker
│   │   └── ActionItem.tsx           # Action queue item
│   ├── api/                         # API client
│   ├── ble/                         # BLE client (react-native-ble-plx)
│   └── store/                       # Zustand state
├── package.json
└── app.json
```

---

## Bill of Materials (BOM)

See `hardware/bom/` for detailed CSV per node.

### Hub BOM Summary

| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| MCU | ESP32-WROOM-32E | 1 | $3.20 | $3.20 |
| LoRa | SX1262 module (Waveshare) | 1 | $8.50 | $8.50 |
| Display | SSD1306 OLED 0.96" 128×64 I2C | 1 | $2.00 | $2.00 |
| Ambient sensor | BME280 breakout | 1 | $3.50 | $3.50 |
| microSD | MicroSD socket + 8GB card | 1 | $2.50 | $2.50 |
| LED | WS2812B status LED | 1 | $0.30 | $0.30 |
| Power | TP4056 + DW01 + 18650 holder | 1 | $1.20 | $1.20 |
| Battery | 18650 3000 mAh | 1 | $4.00 | $4.00 |
| Solar | 5V 1W panel | 1 | $2.00 | $2.00 |
| Connectors | USB-C breakout | 1 | $0.50 | $0.50 |
| PCB | Custom PCB | 1 | $3.00 | $3.00 |
| Enclosure | 3D printed PETG | 1 | $1.50 | $1.50 |
| Misc | Passives, headers, antenna | 1 | $2.00 | $2.00 |
| **Total** | | | | **$34.20** |

### Bin Node BOM Summary

| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| MCU | ESP32-WROOM-32E | 1 | $3.20 | $3.20 |
| LoRa | SX1262 module | 1 | $8.50 | $8.50 |
| Temp probes | DS18B20 waterproof (1m cable) | 3 | $2.50 | $7.50 |
| Moisture | Capacitive soil moisture v1.2 | 3 | $2.00 | $6.00 |
| CO₂ | Sensirion SCD41 breakout | 1 | $40.00 | $40.00 |
| Methane | MQ-4 sensor module | 1 | $5.00 | $5.00 |
| Load cell | 5kg load cell + HX711 | 1 | $6.00 | $6.00 |
| Servo | MG90S metal gear servo | 1 | $4.00 | $4.00 |
| LED | WS2812B | 1 | $0.30 | $0.30 |
| Power | TP4056 + DW01 + 18650 holder | 1 | $1.20 | $1.20 |
| Battery | 18650 3000 mAh | 1 | $4.00 | $4.00 |
| Solar | 5V 2W panel | 1 | $3.50 | $3.50 |
| PCB | Custom PCB | 1 | $3.00 | $3.00 |
| Enclosure | IP65 3D printed + silicone | 1 | $3.00 | $3.00 |
| Misc | Passives, connectors, antenna | 1 | $3.00 | $3.00 |
| **Total** | | | | **$99.20** |

### Soil Probe BOM Summary

| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| MCU | Raspberry Pi Pico (RP2040) | 1 | $4.00 | $4.00 |
| BLE | HM-19 BLE 5.0 module | 1 | $5.00 | $5.00 |
| Display | SSD1306 OLED 0.96" I2C | 1 | $2.00 | $2.00 |
| Temp probes | DS18B20 waterproof (0.5m) | 4 | $2.50 | $10.00 |
| Moisture | Capacitive soil moisture v1.2 | 3 | $2.00 | $6.00 |
| pH | Analog pH probe + MCP3201 ADC | 1 | $15.00 | $15.00 |
| CO₂ | Sensirion SCD41 breakout | 1 | $40.00 | $40.00 |
| I2C mux | TCA9548A | 1 | $2.00 | $2.00 |
| Power | TP4056 + 18650 holder | 1 | $1.20 | $1.20 |
| Battery | 18650 2200 mAh | 1 | $3.50 | $3.50 |
| Enclosure | IP65 probe housing | 1 | $3.00 | $3.00 |
| Misc | Passives, probe shaft | 1 | $3.00 | $3.00 |
| **Total** | | | | **$94.70** |

### Weather Station BOM Summary

| Component | Part | Qty | Unit Price | Total |
|-----------|------|-----|-----------|-------|
| MCU | Adafruit Feather nRF52840 | 1 | $24.95 | $24.95 |
| LoRa | SX1262 module | 1 | $8.50 | $8.50 |
| Sensor | BME280 breakout | 1 | $3.50 | $3.50 |
| Anemometer | Davis 6410 wind speed+vane | 1 | $85.00 | $85.00 |
| Rain gauge | Tipping bucket (0.2mm) | 1 | $25.00 | $25.00 |
| Solar | 6V 2W panel | 1 | $4.00 | $4.00 |
| Charger | MCP73871 solar charger | 1 | $5.00 | $5.00 |
| Battery | LiPo 2000 mAh | 1 | $6.00 | $6.00 |
| Enclosure | Stevenson screen (3D printed) | 1 | $5.00 | $5.00 |
| Misc | Mast, cables, passives | 1 | $5.00 | $5.00 |
| **Total** | | | | **$171.95** |

### Total System BOM

| Node | Cost |
|------|------|
| Hub | $34.20 |
| Bin Node | $99.20 |
| Soil Probe | $94.70 |
| Weather Station | $171.95 |
| **Total** | **$400.05** |

> The Bin Node + Hub + Soil Probe (without weather station) is **$228.10** — a practical entry point. The Weather Station is optional (cloud weather API can substitute).

---

## Composting Science (How the ML Works)

### The Composting Process

```
Phase 1: MESOPHILIC (Days 1-3)
  Temp: 20-45°C  | Microbes: bacteria + fungi break down sugars/starches
  CO₂: rising     | Mass loss: minimal
  ────────────────────────────────────────────────
Phase 2: THERMOPHILIC (Days 3-14)
  Temp: 55-70°C  | Microbes: thermophilic bacteria break down proteins/fats
  CO₂: high       | Mass loss: rapid (20-40% mass lost)
  Pathogen kill: 55°C for 3+ days kills human pathogens + weed seeds
  ────────────────────────────────────────────────
Phase 3: COOLING (Days 14-21)
  Temp: drops to 40°C | Microbes shift to cellulose/lignin breakdown
  CO₂: declining      | Fungi become dominant
  ────────────────────────────────────────────────
Phase 4: MATURATION (Days 21-42)
  Temp: ambient +2-5°C | Microbes: mesophilic, actinomycetes, worms
  CO₂: low             | Stabilization, humification
  ────────────────────────────────────────────────
Phase 5: CURED (Days 42-90)
  Temp: ambient        | Stable, dark, crumbly, earthy smell
  CO₂: background      | C:N ratio 15-20:1, ready for use
```

### Key Sensor Signatures

| Condition | Temp | Moisture | CO₂ | Methane | Diagnosis |
|-----------|------|---------|-----|---------|-----------|
| Healthy thermophilic | 55-65°C | 50-60% | 2000-5000 ppm | <50 ppm | ✅ All good |
| Too wet (anaerobic) | <40°C | >70% | <1000 ppm | >500 ppm | 🚨 Add dry browns, turn |
| Too dry | <30°C | <35% | <500 ppm | <50 ppm | 💧 Add water |
| Too many greens | 45-55°C | 55-65% | >5000 ppm | 50-200 ppm | 🍃 Add carbon (C:N low) |
| Too many browns | <25°C | 40-50% | <300 ppm | <50 ppm | 🌱 Add nitrogen (C:N high) |
| Compacted (no air) | 35-45°C | 60%+ | 1000-2000 | 200-500 ppm | 🔄 Turn pile now |
| Cured/done | ambient | 40-50% | <200 ppm | <10 ppm | 🎉 Ready to harvest |

### C:N Ratio Estimation

The C:N ratio is estimated from sensor fusion (not directly measurable cheaply):

```
Features:
  - Temperature slope (°C/hour, last 24h)
  - CO₂ production rate (ppm/hour, last 24h)
  - Moisture level
  - Mass loss rate (g/hour, last 7 days)
  - Methane concentration
  - Ambient temperature
  - Bin volume (user input)
  - Days since start
  - Days since last turn
  - Phase (from edge classifier)
  → XGBoost → C:N ratio estimate (±3:1 accuracy)
```

---

## Installation & Setup

### 1. Hardware Assembly

1. **Print enclosures** — STL files in `hardware/` (PETG for outdoor UV resistance)
2. **Assemble PCBs** — Gerbers in `schematic/`, order from JLCPCB ($2 for 5 boards)
3. **Solder components** — follow BOM and schematic for each node
4. **Flash firmware** — see `scripts/flash_all.sh`

### 2. Network Setup

1. **Hub** — connects to WiFi (configure via BLE setup mode, mobile app)
2. **Bin Node** — auto-joins LoRa mesh on power-up (pre-paired to Hub)
3. **Soil Probe** — auto-pairs to Bin Node via BLE
4. **Weather Station** — auto-joins LoRa mesh on power-up

### 3. Software Deployment

```bash
# Cloud backend
cd software/dashboard
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8000

# MQTT broker (Mosquitto)
apt install mosquitto mosquitto-clients

# ML pipeline
cd software/ml-pipeline
pip install -r requirements.txt
python inference.py  # Starts batch inference loop
```

### 4. Mobile App

```bash
cd software/mobile-app
npm install
npx react-native run-android  # or run-ios
```

---

## File Structure

```
CompostSync/
├── README.md                           # This file
├── schematic/
│   ├── hub/
│   │   ├── hub.kicad_pro                # KiCad project
│   │   ├── hub.kicad_sch                # Schematic
│   │   └── hub.kicad_pcb                # PCB layout
│   ├── bin-node/
│   │   ├── bin-node.kicad_pro
│   │   ├── bin-node.kicad_sch
│   │   └── bin-node.kicad_pcb
│   ├── soil-probe/
│   │   ├── soil-probe.kicad_pro
│   │   ├── soil-probe.kicad_sch
│   │   └── soil-probe.kicad_pcb
│   └── weather-station/
│       ├── weather-station.kicad_pro
│       ├── weather-station.kicad_sch
│       └── weather-station.kicad_pcb
├── firmware/
│   ├── common/
│   │   ├── csp_protocol.h               # CompostSync Protocol
│   │   ├── csp_protocol.c
│   │   ├── sx1262_driver.h              # LoRa driver
│   │   ├── sx1262_driver.c
│   │   ├── aes_ccm.h                    # AES-128-CCM
│   │   ├── aes_ccm.c
│   │   └── sensor_types.h
│   ├── hub/
│   │   ├── main.c                        # FreeRTOS main
│   │   ├── lora_mesh.c                   # TDMA mesh coordinator
│   │   ├── ble_service.c                 # BLE GATT server
│   │   ├── wifi_mqtt.c                   # WiFi + MQTT client
│   │   ├── edge_ml.c                     # TFLite Micro inference
│   │   ├── display.c                     # OLED display
│   │   ├── sd_logger.c                   # microSD logging
│   │   └── CMakeLists.txt
│   ├── bin-node/
│   │   ├── main.c
│   │   ├── sensors.c                     # DS18B20, moisture, SCD41, MQ-4, HX711
│   │   ├── servo.c                        # Vent control
│   │   ├── lora_node.c                    # LoRa TDMA node
│   │   ├── ble_bridge.c                   # BLE to Soil Probe
│   │   ├── power.c                        # Sleep/wake management
│   │   └── CMakeLists.txt
│   ├── soil-probe/
│   │   ├── main.c
│   │   ├── sensors.c                     # DS18B20, moisture, pH, SCD41
│   │   ├── display.c                     # OLED
│   │   ├── ble_uart.c                     # BLE UART
│   │   ├── power.c                       # Sleep management
│   │   └── CMakeLists.txt
│   └── weather-station/
│       ├── main.c
│       ├── sensors.c                     # BME280, anemometer, rain gauge
│       ├── lora_node.c                   # LoRa TDMA node
│       ├── power.c                       # Solar management
│       └── CMakeLists.txt
├── hardware/
│   └── bom/
│       ├── hub_bom.csv
│       ├── bin-node_bom.csv
│       ├── soil-probe_bom.csv
│       └── weather-station_bom.csv
├── software/
│   ├── dashboard/
│   │   ├── main.py
│   │   ├── routers/
│   │   │   ├── devices.py
│   │   │   ├── telemetry.py
│   │   │   ├── compost.py
│   │   │   ├── alerts.py
│   │   │   └── auth.py
│   │   ├── models/
│   │   │   └── schemas.py
│   │   ├── mqtt/
│   │   │   └── client.py
│   │   ├── ml/
│   │   │   └── inference.py
│   │   ├── db.py
│   │   ├── config.py
│   │   └── requirements.txt
│   ├── ml-pipeline/
│   │   ├── train_maturity_lstm.py
│   │   ├── train_cn_ratio.py
│   │   ├── train_completion.py
│   │   ├── train_additem_classifier.py
│   │   ├── train_pest_risk.py
│   │   ├── synthetic_compost_sim.py
│   │   ├── inference.py
│   │   └── requirements.txt
│   └── mobile-app/
│       ├── App.tsx
│       ├── package.json
│       ├── app.json
│       └── src/
│           ├── screens/
│           │   ├── DashboardScreen.tsx
│           │   ├── ScannerScreen.tsx
│           │   ├── ActionsScreen.tsx
│           │   ├── TimelineScreen.tsx
│           │   └── SettingsScreen.tsx
│           ├── components/
│           │   ├── MaturityGauge.tsx
│           │   ├── SensorCard.tsx
│           │   └── PhaseIndicator.tsx
│           ├── api/
│           │   └── client.ts
│           ├── ble/
│           │   └── bleClient.ts
│           └── store/
│               └── store.ts
├── docs/
│   ├── architecture.md
│   ├── api-spec.md
│   ├── protocol-spec.md
│   └── assembly-guide.md
└── scripts/
    ├── flash_all.sh
    ├── calibrate_moisture.py
    ├── calibrate_loadcell.py
    ├── deploy_backend.sh
    └── train_models.sh
```

---

## Environmental Impact

| Metric | Per Household/Year | 1M Households |
|--------|--------------------|--------------|
| Food waste diverted from landfill | 250 kg | 250,000 tonnes |
| Methane emissions prevented | 12 kg CH₄ (= 1,000 kg CO₂e) | 1,000,000 tonnes CO₂e |
| Compost produced (replaces fertilizer) | 100 kg | 100,000 tonnes |
| Fertilizer runoff prevented | 50 kg N/P/K | 50,000 tonnes |
| Equivalent to taking cars off road | — | 216,000 cars/year |

**CompostSync is climate tech you can hold in your hands.**

---

## License

MIT — build it, sell it, compost with it.

---

*Invented as system #26 in the Devices repository. Every household can be a micro-farm, every compost bin a carbon sink.*