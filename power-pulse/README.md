# PowerPulse — AI-Powered Home Energy Intelligence & Electrical Safety System

> **Stop guessing about your electricity bill. Stop worrying about electrical fires. PowerPulse watches every circuit, tags every appliance, optimizes every watt — and warns you before anything goes wrong.**

PowerPulse is a full-stack, multi-node home energy monitoring and optimization system. It combines per-circuit current/voltage sensing, plug-level appliance tagging via BLE beacons, solar/battery integration, and cloud ML to give homeowners unprecedented visibility and control over their electrical life. It detects anomalies (arcing, overheating, phantom loads), predicts bills, optimizes solar self-consumption, and can even shed non-critical loads during peak pricing or outages.

---

## Why PowerPulse?

- **68% of home electrical fires** start in wiring or panels that homeowners never inspect. Arc-fault detection saves lives.
- **Phantom loads** (idle TVs, chargers, game consoles) waste 5–10% of residential electricity — that's $100–$300/year per household.
- **Time-of-use rate optimization** can save 15–25% on electricity bills with automated load shifting.
- **Solar + battery owners** need intelligent self-consumption optimization — most waste 30–40% of their generation without it.
- **No existing solution** combines per-circuit monitoring, appliance-level disaggregation, arc detection, solar optimization, and predictive alerts in one open system.

---

## System Architecture

```
                          ┌──────────────────┐
                          │   Cloud (AWS)    │
                          │  FastAPI + ML    │
                          │  TimescaleDB     │
                          │  MQTT Broker     │
                          └──────┬───────────┘
                                 │ TLS/MQTT
                                 │
                          ┌──────┴───────────┐
                          │   Hub Node       │
                          │  ESP32-S3        │
                          │  WiFi + BLE      │
                          │  MQTT Gateway    │
                          │  Local Inference │
                          └──┬────┬────┬─────┘
                             │    │    │
                  ┌──────────┘    │    └──────────┐
                  │               │               │
           ┌──────┴──────┐ ┌─────┴─────┐ ┌──────┴──────┐
           │ Circuit     │ │ Appliance │ │ Solar/Batt  │
           │ Monitor     │ │ Tag ×N    │ │ Node        │
           │ STM32G4     │ │ nRF52840  │ │ RP2040      │
           │ CT sensors  │ │ BLE beacon│ │ MPPT ctrl   │
           │ Arc detect  │ │ relay+LED │ │ BMS monitor │
           └─────────────┘ └───────────┘ └─────────────┘
            Sub-GHz 868MHz    BLE 2.4GHz    Sub-GHz 868MHz
```

### Communication Fabric

| Link | Protocol | Band | Range | Data Rate |
|------|----------|------|-------|-----------|
| Hub ↔ Circuit Monitor | Sub-GHz (CC1101) | 868 MHz | 30 m (in-panel) | 10 kbps |
| Hub ↔ Appliance Tag | BLE 5.0 mesh | 2.4 GHz | 10 m (room) | 125 kbps |
| Hub ↔ Solar Node | Sub-GHz (CC1101) | 868 MHz | 50 m (rooftop) | 10 kbps |
| Hub ↔ Cloud | WiFi (ESP32-S3) | 2.4 GHz | Internet | Variable |

---

## Hardware Nodes

### 1. Hub Node — Central Coordinator

The brain of the system. Lives in a wall-mounted enclosure near the breaker panel. Aggregates data from all nodes, runs local inference for time-critical alerts (arc fault, overload), manages MQTT bridge to the cloud, and exposes a local web dashboard for when the internet is down.

| Component | Part | Function |
|-----------|------|----------|
| MCU | ESP32-S3-WROOM-1-N16R8 | Main processor, WiFi, BLE, 16MB flash, 8MB PSRAM |
| Sub-GHz Radio | CC1101 | 868 MHz transceiver for circuit monitors & solar node |
| RTC | DS3231 | Accurate timestamping even without WiFi |
| Display | 2.8" ILI9341 TFT | Local status display (optional) |
| Storage | microSD slot | Local data buffer (offline operation) |
| Power | 5V USB-C + 3.7V 18650 backup | Main + battery backup |
| LEDs | WS2812B RGB | Status indicators |
| Buzzer | Piezo 3.3V | Audible alarms |
| USB | CH340C | Debug/programming port |

**Pin Assignments (ESP32-S3):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO4 | CC1101_SPI_CLK | CC1101 SCLK |
| GPIO5 | CC1101_SPI_MISO | CC1101 MISO |
| GPIO6 | CC1101_SPI_MOSI | CC1101 MOSI |
| GPIO7 | CC1101_CS | CC1101 CSN |
| GPIO8 | CC1101_GDO0 | CC1101 GDO0 (IRQ) |
| GPIO9 | CC1101_GDO2 | CC1101 GDO2 (RX ready) |
| GPIO10 | DS3231_SDA | DS3231 SDA |
| GPIO11 | DS3231_SCL | DS3231 SCL |
| GPIO12 | SD_SPI_CLK | microSD SCLK |
| GPIO13 | SD_SPI_MISO | microSD MISO |
| GPIO14 | SD_SPI_MOSI | microSD MOSI |
| GPIO15 | SD_CS | microSD CS |
| GPIO16 | TFT_DC | ILI9341 DC |
| GPIO17 | TFT_CS | ILI9341 CS |
| GPIO18 | TFT_RST | ILI9341 RESET |
| GPIO19 | TFT_SCLK | ILI9341 SCLK (SPI3) |
| GPIO20 | TFT_MOSI | ILI9341 MOSI |
| GPIO21 | WS2812B_DATA | Status LED chain |
| GPIO35 | BUZZER | Piezo buzzer |
| GPIO36 | BUTTON_1 | Config button |
| GPIO37 | BUTTON_2 | Reset button |
| GPIO38 | BAT_SENSE | 18650 voltage divider (ADC) |
| GPIO43 | USB_TX | CH340C TX |
| GPIO44 | USB_RX | CH340C RX |

**Power Architecture:**
- Main: USB-C 5V → MP28167 buck → 3.3V rail (2A peak)
- Backup: 18650 (3.7V 2600mAh) → TP4056 charger + DW01A protection → same 3.3V rail via auto-switch
- Battery provides ~4 hours offline operation

### 2. Circuit Monitor — Per-Circuit Energy Sensing

Installs inside the breaker panel (one per panel, monitors up to 16 circuits). Uses clamp-on CT sensors on each breaker wire. Detects arc faults, measures per-circuit voltage, current, power factor, and energy. Communicates via Sub-GHz to the hub.

| Component | Part | Function |
|-----------|------|----------|
| MCU | STM32G474RET6 | 170 MHz Cortex-M4, 512KB flash, DSP instructions |
| ADC | ADS131E08 | 8-channel 24-bit simultaneous sampling ADC |
| CT Inputs | 16× SCT-013-030 | 30A split-core current transformers |
| Voltage Sense | Transformer + op-amp circuit | Mains voltage measurement |
| Sub-GHz Radio | CC1101 | 868 MHz link to hub |
| Arc Detection | Digital filter on STM32 | High-frequency arc signature detection |
| Isolation | ISO7741 | SPI isolation barrier |
| Power | 120/240V AC→5V (HLK-PM03) | Panel-powered, isolated |
| EEPROM | AT24C256 | Calibration data persistence |
| LEDs | 3× 0805 LED (R/G/B) | Status |
| Temp Sensor | TMP117 | Panel temperature monitoring |

**Pin Assignments (STM32G474):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| PA0 | ADC_CH0 | ADS131E08 MISO (via ISO7741) |
| PA1 | ADC_CH1 | ADS131E08 SCLK (via ISO7741) |
| PA2 | ADC_CH2 | ADS131E08 MOSI (via ISO7741) |
| PA3 | ADC_CH3 | ADS131E08 CS (via ISO7741) |
| PA4 | ADC_CH4 | ADS131E08 DRDY (via ISO7741) |
| PA5 | SPI1_SCK | CC1101 SCLK |
| PA6 | SPI1_MISO | CC1101 MISO |
| PA7 | SPI1_MOSI | CC1101 MOSI |
| PA8 | CC1101_CS | CC1101 CSN |
| PA9 | CC1101_GDO0 | CC1101 GDO0 |
| PA10 | CC1101_GDO2 | CC1101 GDO2 |
| PA11 | I2C1_SDA | AT24C256 SDA |
| PA12 | I2C1_SCL | AT24C256 SCL |
| PB0 | I2C2_SDA | TMP117 SDA |
| PB1 | I2C2_SCL | TMP117 SCL |
| PB2 | BOOT1 | Boot config |
| PB3 | LED_R | Red status LED |
| PB4 | LED_G | Green status LED |
| PB5 | LED_B | Blue status LED |
| PB6 | ADC_V_SENSE | Mains voltage sense (scaled) |
| PB7 | ADC_TEMP | Internal MCU temp |
| PC0 | ADS131E08_START | Start conversion |
| PC1 | ADS131E08_RESET | Reset ADC |
| PC8 | VBUS_SENSE | AC power present detect |

**Arc Fault Detection Algorithm:**
The STM32G474 runs a real-time arc detection algorithm:
1. Sample each CT at 4 kHz (oversampled from ADS131E08 at 8 kHz, decimated)
2. Compute short-time FFT on 256-sample windows (64 ms)
3. Detect characteristic arc signatures: broadband high-frequency energy above 10 kHz
4. Apply moving-window energy ratio test (burst energy / baseline)
5. If ratio exceeds threshold for ≥3 consecutive windows → ARC FAULT confirmed
6. Send immediate Sub-GHz alert to Hub, which triggers local alarm and cloud notification

**Power Architecture:**
- AC mains → HLK-PM03 (85-264VAC → 5V/600mA, fully isolated)
- 5V → AP2112 LDO → 3.3V rail
- Isolation boundary: all CT and voltage inputs are isolated via ISO7741 to the ADC, ADC SPI crosses isolation via digital isolators
- Total budget: ~350mA @ 3.3V (well within supply)

### 3. Appliance Tag — Plug-Level Monitor & Control

Small BLE beacons that plug into wall outlets or inline with appliances. Measure per-appliance energy (voltage, current, power), provide on/off relay control, and broadcast consumption data to the hub via BLE mesh. Each tag has a unique ID and can be assigned to rooms/appliances via the mobile app.

| Component | Part | Function |
|-----------|------|----------|
| MCU | nRF52840 | BLE 5.0, 64 MHz Cortex-M4F, 1MB flash, 256KB RAM |
| Energy IC | BL0937 | Single-phase energy measurement (V, I, PF, W) |
| Relay | Omron G3MB-202P | 2A solid-state relay (on/off control) |
| Display | 0.91" SSD1306 OLED | Shows current watts |
| Button | Tactile 6mm | Manual toggle + BLE pairing |
| LEDs | WS2812B-mini | Status LED (single) |
| Power | AC→5V (HLK-PM01) → 3.3V LDO | Plug-powered |
| Antenna | PCB trace (2.4 GHz) | BLE antenna |
| EEPROM | None (internal flash) | Settings in nRF flash |

**Pin Assignments (nRF52840):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02 | BL0937_CF | Power pulse output |
| P0.03 | BL0937_CF1 | Current/voltage select |
| P0.04 | BL0937_SEL | CF1 mode select |
| P0.05 | RELAY_CTRL | G3MB-202P input |
| P0.06 | BUTTON | Manual toggle (active low) |
| P0.07 | WS2812B | Status LED data |
| P0.08 | I2C_SDA | SSD1306 SDA |
| P0.09 | I2C_SCL | SSD1306 SCL |
| P0.11 | SWDIO | Debug |
| P0.12 | SWCLK | Debug |
| P0.18 | RESET | System reset |
| P0.20 | BLE_ADV | BLE advertising LED (active) |
| P0.22 | CAL_PIN | Calibration trigger |

**Form Factor:** 45mm × 55mm PCB, fits inside standard plug-through housing (like a smart plug but with BLE mesh instead of WiFi for zero-router-dependency operation).

### 4. Solar Node — MPPT Controller & Battery Monitor

Installs near the solar panel / battery bank. Implements maximum power point tracking (MPPT) for up to 600W of solar panels, monitors battery state of charge, and coordinates with the hub for optimal self-consumption scheduling.

| Component | Part | Function |
|-----------|------|----------|
| MCU | RP2040 | Dual-core M0+, 133 MHz, flexible PIO for MPPT PWM |
| MPPT Controller | Custom buck converter + RP2040 PIO | MPPT algorithm driving synchronous buck |
| Solar Input | 600W max (30V/20A) | MC4 connectors |
| Battery | 48V LiFePO4 | CAN bus to BMS |
| BMS Interface | LTC6811 | 48V battery cell monitoring (16S) |
| Current Sense | INA260 (solar) + INA260 (load) | Bidirectional current sensing |
| Voltage Dividers | Resistor networks | Solar voltage, battery voltage |
| Sub-GHz Radio | CC1101 | 868 MHz link to hub |
| Display | 1.3" SH1106 OLED | Status display |
| Thermocouple | MAX31855 + K-type | Heatsink temp monitoring |
| Fan Control | MOSFET + 12V fan | Active cooling |
| Power | Solar/battery → 5V buck → 3.3V LDO | Self-powered from battery |
| USB | Micro USB | Debug/programming |

**Pin Assignments (RP2040):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GP0 | I2C0_SDA | INA260 #1 (solar) SDA |
| GP1 | I2C0_SCL | INA260 #1 SCL |
| GP2 | I2C1_SDA | INA260 #2 (load) SDA |
| GP3 | I2C1_SCL | INA260 #2 SCL |
| GP4 | SPI0_SCK | CC1101 SCLK |
| GP5 | SPI0_MISO | CC1101 MISO |
| GP6 | SPI0_MOSI | CC1101 MOSI |
| GP7 | SPI0_CS | CC1101 CSN |
| GP8 | CC1101_GDO0 | CC1101 GDO0 |
| GP9 | CC1101_GDO2 | CC1101 GDO2 |
| GP10 | PWM_MPPT | Synchronous buck high-side gate |
| GP11 | PWM_MPPT_COMP | Synchronous buck low-side gate |
| GP12 | SPI1_SCK | MAX31855 SCLK |
| GP13 | SPI1_MISO | MAX31855 MISO (thermocouple) |
| GP14 | SPI1_CS | MAX31855 CS |
| GP15 | FAN_PWM | Cooling fan PWM |
| GP16 | I2C0_SDA2 | SH1106 OLED SDA |
| GP17 | I2C0_SCL2 | SH1106 OLED SCL |
| GP18 | CAN_TX | BMS CAN bus TX (via MCP2551) |
| GP19 | CAN_RX | BMS CAN bus RX (via MCP2551) |
| GP20 | ADC_SOLAR_V | Solar voltage divider (ADC) |
| GP21 | ADC_BATT_V | Battery voltage divider (ADC) |
| GP22 | LED_STATUS | Onboard LED |
| GP26 | ADC_TEMP | Heatsink thermistor |
| GP27 | EMERGENCY_SHUTDOWN | Hardware shutdown (active high) |

**MPPT Algorithm (Perturb & Observe with Incremental Conductance enhancement):**
1. Measure solar voltage and current via INA260 (sampled at 100 Hz)
2. Compute power: P = V × I
3. Compare to previous: if dP/dV > 0 → increase duty, if dP/dV < 0 → decrease duty
4. Enhanced: use incremental conductance (I/V + dI/dV) to handle rapidly changing irradiance
5. PIO generates complementary PWM at 100 kHz for synchronous buck converter
6. Dead time: 50ns (configured in PIO program)
7. Duty cycle limits: 5%–95% (hardware constraint for buck converter)
8. Maximum current limiting via INA260 feedback (software current mode)

---

## Firmware Architecture

### Common Protocol — `powerpulse_common`

All nodes share a common header defining the wireless protocol:

```
┌─────────────────────────────────────────────────────────────┐
│ PowerPulse Wireless Frame (Sub-GHz)                        │
├──────┬──────┬──────┬──────┬──────┬──────┬──────┬───────────┤
│ SOF  │ LEN  │ SRC  │ DST  │ TYPE │ SEQ  │ CRC  │ PAYLOAD   │
│ 0xAA │ 1B   │ 2B   │ 2B   │ 1B   │ 2B   │ 2B   │ 0-200B   │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴───────────┘
│ SOF  │ LEN  │ SRC  │ DST  │ TYPE │ SEQ  │ CRC  │ PAYLOAD   │
│ 0xAA │ 1B   │ 2B   │ 2B   │ 1B   │ 2B   │ 2B   │ 0-200B   │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴───────────┘

Message Types (TYPE field):
  0x01 — HEARTBEAT (node alive, battery %, uptime)
  0x02 — CIRCUIT_DATA (16 circuits: V, I, PF, W per circuit)
  0x03 — ARC_FAULT_ALERT (circuit #, confidence %, timestamp)
  0x04 — APPLIANCE_DATA (tag ID, V, I, PF, W, cumulative Wh)
  0x05 — APPLIANCE_CMD (tag ID, relay on/off, schedule)
  0x06 — SOLAR_DATA (PV V/I/P, battery SoC, charge mode)
  0x07 — SOLAR_CMD (target duty %, mode override, emergency)
  0x08 — CALIBRATION (node ID, calibration parameters)
  0x09 — OTA_UPDATE (firmware chunk, CRC32 per chunk)
  0x0A — OVERLOAD_ALERT (circuit #, current %, timestamp)
  0x0B — TIME_SYNC (unix timestamp from hub)
  0xFF — ACK (acknowledges received message)
```

BLE mesh protocol uses similar structure but with BLE mesh model IDs:
- `0x1001` — PowerPulse Appliance Server (reports energy data)
- `0x1002` — PowerPulse Appliance Client (sends relay commands)
- `0x1003` — PowerPulse Alert Server (sends arc/overload alerts)

### Firmware Components

#### Hub Node (`firmware/hub-node/`)
- `main.c` — FreeRTOS task scheduler, WiFi/BLE/Sub-GHz initialization
- `wifi_task.c` — WiFi STA connection, MQTT client, OTA
- `ble_mesh_task.c` — BLE mesh provisioning, appliance tag management
- `subghz_task.c` — Sub-GHz transceiver driver, packet TX/RX
- `cloud_task.c` — MQTT publish/subscribe, JSON encoding, batch upload
- `inference_task.c` — Local TinyML anomaly detection (TensorFlow Lite Micro)
- `display_task.c` — ILI9341 dashboard rendering
- `alert_task.c` — Buzzer, LED, push notification management
- `config_task.c` — NVS configuration, WiFi setup, node pairing
- `sd_task.c` — Offline data buffering to microSD
- `CMakeLists.txt` — ESP-IDF build

#### Circuit Monitor (`firmware/circuit-monitor/`)
- `main.c` — STM32 HAL init, FreeRTOS scheduler
- `adc_task.c` — ADS131E08 driver, 8 kHz sampling, per-circuit demux
- `arc_detect_task.c` — Real-time arc fault detection (FFT + ratio test)
- `power_calc_task.c` — RMS voltage/current, real power, power factor, energy accumulation
- `subghz_task.c` — CC1101 driver, packet framing, TX scheduling
- `calibration_task.c` — CT calibration, zero-offset correction, gain matching
- `temp_task.c` — Panel temperature monitoring, overtemp alert
- `overload_detect.c` — Overcurrent detection per circuit (configurable thresholds)
- `Makefile` — ARM GCC build

#### Appliance Tag (`firmware/appliance-tag/`)
- `main.c` — nRF52 SoftDevice init, BLE mesh provisioning
- `energy_task.c` — BL0937 pulse counting, power calculation
- `relay_task.c` — Relay control, zero-cross switching
- `ble_mesh_task.c` — BLE mesh model handlers, data publishing
- `button_task.c` — Debounced button handler (toggle + long-press pairing)
- `display_task.c` — SSD1306 OLED rendering (power display)
- `calibration.c` — BL0937 calibration routine
- `CMakeLists.txt` — nRF Connect SDK build

#### Solar Node (`firmware/solar-node/`)
- `main.c` — RP2040 dual-core init, core0=comms, core1=MPPT
- `mppt_task.c` — Core 1: P&O + IC MPPT algorithm, PIO PWM generation
- `bms_task.c` — LTC6811 cell monitoring, CAN communication
- `solar_task.c` — INA260 solar power measurement
- `load_task.c` — INA260 load side measurement, net metering
- `subghz_task.c` — CC1101 driver, data reporting to hub
- `thermal_task.c` — MAX31855 heatsink monitoring, fan PWM
- `display_task.c` — SH1106 OLED status display
- `safety_task.c` — Overvoltage, overcurrent, overtemp shutdown
- `CMakeLists.txt` — Pico SDK build

---

## Cloud Software

### FastAPI Backend (`software/dashboard/`)

```
software/dashboard/
├── app/
│   ├── main.py              # FastAPI app, MQTT client, startup
│   ├── config.py             # Settings, secrets, MQTT broker URL
│   ├── models.py             # SQLAlchemy models (TimescaleDB hypertables)
│   ├── routers/
│   │   ├── energy.py          # Energy data endpoints (GET /energy/circuits, /energy/appliances)
│   │   ├── alerts.py          # Alert endpoints (GET /alerts, POST /alerts/{id}/acknowledge)
│   │   ├── devices.py         # Device management (GET/POST /devices, /devices/{id}/calibrate)
│   │   ├── solar.py           # Solar data endpoints (GET /solar/production, /solar/battery)
│   │   ├── automation.py      # Automation rules (CRUD /automation/rules)
│   │   └── billing.py         # Bill estimation, TOU optimization
│   ├── services/
│   │   ├── mqtt_handler.py    # MQTT message routing, JSON parsing
│   │   ├── anomaly_detector.py # ML-based anomaly detection service
│   │   ├── bill_estimator.py  # Time-of-use rate calculator
│   │   ├── disaggregator.py   # NILM appliance disaggregation
│   │   └── solar_optimizer.py  # Solar self-consumption optimizer
│   ├── ml/
│   │   ├── arc_model.py       # Arc fault classifier (pre-trained, served)
│   │   ├── load_model.py      # Appliance load signature model
│   │   └── forecast_model.py  # Energy consumption forecaster
│   └── database.py            # TimescaleDB connection, migrations
├── requirements.txt
├── Dockerfile
└── docker-compose.yml
```

**Key API Endpoints:**

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/energy/circuits` | Per-circuit power data (live + historical) |
| GET | `/api/v1/energy/appliances` | Per-appliance power data (live + historical) |
| GET | `/api/v1/energy/total` | Whole-home aggregate power |
| GET | `/api/v1/alerts` | Active and recent alerts |
| POST | `/api/v1/alerts/{id}/acknowledge` | Acknowledge an alert |
| GET | `/api/v1/devices` | List all registered nodes |
| POST | `/api/v1/devices/{id}/command` | Send command to a node (relay toggle, etc.) |
| GET | `/api/v1/solar/production` | Solar production data |
| GET | `/api/v1/solar/battery` | Battery SoC and health |
| POST | `/api/v1/automation/rules` | Create automation rule |
| GET | `/api/v1/billing/estimate` | Estimated monthly bill |
| GET | `/api/v1/billing/tou-schedule` | Time-of-use rate schedule |
| POST | `/api/v1/billing/optimize` | Get load-shifting recommendations |

### ML Pipeline (`software/ml-pipeline/`)

```
software/ml-pipeline/
├── train_arc_detector.py     # Train arc fault classifier on circuit data
├── train_appliance_nilm.py   # Train appliance disaggregation model
├── train_consumption_forecast.py  # Train energy consumption forecaster
├── train_anomaly_detector.py     # Train anomaly detection autoencoder
├── models/
│   ├── arc_detector.tflite    # Quantized TFLite model for hub
│   ├── nilm_model.onnx        # Appliance disaggregation
│   ├── forecast_model.onnx    # Consumption forecasting
│   └── anomaly_ae.tflite      # Anomaly detection for hub
├── datasets/
│   ├── README.md              # Dataset format documentation
│   └── sample/                # Sample data for testing
├── features/
│   ├── circuit_features.py    # Feature engineering from raw circuit data
│   ├── appliance_features.py  # Appliance signature features
│   └── time_features.py       # Temporal features (hour, day, season)
├── evaluate.py                # Model evaluation metrics
├── export_tflite.py           # Export to TFLite for edge deployment
└── requirements.txt
```

**ML Models:**

1. **Arc Fault Classifier** (runs on hub, TFLite Micro)
   - Input: 256-sample current waveform window (8 kHz, 32 ms)
   - Architecture: 1D CNN (3 conv layers) → 2-class output (normal / arc)
   - Accuracy target: >99.5% (false negative rate <0.01%)
   - Inference: <5 ms on ESP32-S3

2. **Appliance Disaggregator** (runs on cloud, ONNX)
   - Input: Aggregate power waveform (1 Hz, 24-hour window)
   - Architecture: Sequence-to-point CNN + attention
   - Output: Per-appliance power traces
   - Appliance classes: HVAC, EV charger, water heater, oven, dryer, fridge, lighting, other

3. **Consumption Forecaster** (runs on cloud, ONNX)
   - Input: 7 days of hourly consumption + weather forecast + calendar features
   - Architecture: Temporal Fusion Transformer (simplified)
   - Output: 48-hour consumption forecast (hourly bins)

4. **Anomaly Autoencoder** (runs on hub, TFLite Micro)
   - Input: 60-second power window (per circuit, 1 Hz)
   - Architecture: LSTM autoencoder (32-unit encoder, 16-unit latent)
   - Output: Reconstruction error → anomaly score
   - Detects: unusual loads, failing appliances, phantom load changes

### Mobile App (`software/mobile-app/`)

```
software/mobile-app/
├── App.tsx
├── src/
│   ├── screens/
│   │   ├── DashboardScreen.tsx    # Real-time power flow visualization
│   │   ├── CircuitsScreen.tsx      # Per-circuit monitoring
│   │   ├── AppliancesScreen.tsx    # Appliance tags + relay control
│   │   ├── SolarScreen.tsx         # Solar production + battery
│   │   ├── AlertsScreen.tsx        # Active alerts, history
│   │   ├── BillingScreen.tsx       # Bill estimate, TOU schedule
│   │   ├── AutomationScreen.tsx    # Rule creation/management
│   │   └── SettingsScreen.tsx      # Device pairing, WiFi config
│   ├── components/
│   │   ├── PowerFlowCard.tsx       # Animated power flow visualization
│   │   ├── CircuitGauge.tsx        # Per-circuit power gauge
│   │   ├── ApplianceCard.tsx        # Appliance card with toggle
│   │   ├── SolarChart.tsx          # Production vs consumption chart
│   │   ├── AlertBadge.tsx          # Alert notification badge
│   │   └── BillProjectionCard.tsx  # Monthly bill projection
│   ├── services/
│   │   ├── mqtt.ts                 # MQTT over WebSocket client
│   │   ├── api.ts                  # REST API client
│   │   └── ble.ts                  # BLE pairing for appliance tags
│   ├── hooks/
│   │   ├── useEnergy.ts            # Real-time energy data hook
│   │   ├── useAlerts.ts            # Alert notification hook
│   │   └── useSolar.ts             # Solar data hook
│   └── theme.ts
├── package.json
├── tsconfig.json
└── app.json
```

---

## Bill of Materials

### Hub Node BOM

| # | Part | Qty | Unit Cost | Total | Source |
|---|------|-----|-----------|-------|--------|
| 1 | ESP32-S3-WROOM-1-N16R8 | 1 | $5.50 | $5.50 | Mouser |
| 2 | CC1101 module (868 MHz) | 1 | $2.80 | $2.80 | AliExpress |
| 3 | DS3231 RTC module | 1 | $1.20 | $1.20 | LCSC |
| 4 | ILI9341 2.8" TFT (optional) | 1 | $4.50 | $4.50 | AliExpress |
| 5 | microSD card socket | 1 | $0.40 | $0.40 | LCSC |
| 6 | CH340C USB-UART | 1 | $0.60 | $0.60 | LCSC |
| 7 | WS2812B RGB LED | 3 | $0.15 | $0.45 | AliExpress |
| 8 | Piezo buzzer 3.3V | 1 | $0.30 | $0.30 | LCSC |
| 9 | MP28167 buck converter | 1 | $0.80 | $0.80 | Mouser |
| 10 | TP4056 + DW01A battery mgmt | 1 | $0.50 | $0.50 | LCSC |
| 11 | 18650 battery holder | 1 | $0.30 | $0.30 | LCSC |
| 12 | Samsung 18650 2600mAh | 1 | $4.00 | $4.00 | Battery Depot |
| 13 | USB-C connector | 1 | $0.20 | $0.20 | LCSC |
| 14 | Tactile buttons (2×) | 2 | $0.05 | $0.10 | LCSC |
| 15 | Resistors/caps (assorted) | 1 | $0.50 | $0.50 | Assorted |
| 16 | 4-layer PCB (50×80mm) | 1 | $3.00 | $3.00 | JLCPCB |
| 17 | Enclosure (3D printed) | 1 | $2.00 | $2.00 | DIY |
| | **TOTAL** | | | **$27.15** | |

### Circuit Monitor BOM

| # | Part | Qty | Unit Cost | Total | Source |
|---|------|-----|-----------|-------|--------|
| 1 | STM32G474RET6 | 1 | $4.20 | $4.20 | Mouser |
| 2 | ADS131E08IPHP (TQFP-48) | 1 | $6.50 | $6.50 | Mouser |
| 3 | SCT-013-030 (30A CT) | 16 | $3.50 | $56.00 | DigiKey |
| 4 | CC1101 module (868 MHz) | 1 | $2.80 | $2.80 | AliExpress |
| 5 | ISO7741 quad digital isolator | 3 | $1.80 | $5.40 | Mouser |
| 6 | HLK-PM03 (AC-DC 5V/600mA) | 1 | $2.50 | $2.50 | AliExpress |
| 7 | AP2112 3.3V LDO | 1 | $0.30 | $0.30 | LCSC |
| 8 | AT24C256 EEPROM | 1 | $0.40 | $0.40 | LCSC |
| 9 | TMP117 temperature sensor | 1 | $1.20 | $1.20 | Mouser |
| 10 | Voltage transformer (small PT) | 1 | $3.00 | $3.00 | LCSC |
| 11 | Op-amp circuit (LM358 + passives) | 1 | $0.80 | $0.80 | LCSC |
| 12 | 0805 LEDs (R/G/B) | 3 | $0.05 | $0.15 | LCSC |
| 13 | SMA antenna connector | 1 | $0.30 | $0.30 | LCSC |
| 14 | Wire terminal blocks (16×) | 2 | $1.00 | $2.00 | LCSC |
| 15 | 4-layer PCB (100×120mm) | 1 | $5.00 | $5.00 | JLCPCB |
| 16 | Enclosure (in-panel DIN mount) | 1 | $3.00 | $3.00 | DIY |
| 17 | Resistors/caps (assorted) | 1 | $1.00 | $1.00 | Assorted |
| | **TOTAL** | | | **$94.35** | |

### Appliance Tag BOM

| # | Part | Qty | Unit Cost | Total | Source |
|---|------|-----|-----------|-------|--------|
| 1 | nRF52840 module (E73-2G4M08S1C) | 1 | $3.80 | $3.80 | Ebyte/Mouser |
| 2 | BL0937 energy metering IC | 1 | $0.60 | $0.60 | LCSC |
| 3 | Omron G3MB-202P solid-state relay | 1 | $1.80 | $1.80 | DigiKey |
| 4 | SSD1306 0.91" OLED (I2C) | 1 | $1.50 | $1.50 | AliExpress |
| 5 | WS2812B-mini LED | 1 | $0.10 | $0.10 | AliExpress |
| 6 | Tactile button 6mm | 1 | $0.05 | $0.05 | LCSC |
| 7 | HLK-PM01 (AC-DC 5V/600mA) | 1 | $1.50 | $1.50 | AliExpress |
| 8 | AMS1117-3.3 LDO | 1 | $0.15 | $0.15 | LCSC |
| 9 | Shunt resistor 1mΩ (current sense) | 1 | $0.10 | $0.10 | LCSC |
| 10 | Voltage divider resistors | 1 | $0.05 | $0.05 | Assorted |
| 11 | 2-layer PCB (45×55mm) | 1 | $1.50 | $1.50 | JLCPCB |
| 12 | Plug-through housing | 1 | $1.50 | $1.50 | AliExpress |
| 13 | Resistors/caps (assorted) | 1 | $0.30 | $0.30 | Assorted |
| | **TOTAL** | | | **$12.95** | |

### Solar Node BOM

| # | Part | Qty | Unit Cost | Total | Source |
|---|------|-----|-----------|-------|--------|
| 1 | RP2040 | 1 | $1.00 | $1.00 | Mouser |
| 2 | W25Q16 (2MB flash) | 1 | $0.30 | $0.30 | LCSC |
| 3 | CC1101 module (868 MHz) | 1 | $2.80 | $2.80 | AliExpress |
| 4 | INA260 current sensor (×2) | 2 | $2.50 | $5.00 | Mouser |
| 5 | MAX31855 thermocouple IC | 1 | $3.00 | $3.00 | Mouser |
| 6 | K-type thermocouple | 1 | $1.00 | $1.00 | AliExpress |
| 7 | MCP2551 CAN transceiver | 1 | $0.80 | $0.80 | LCSC |
| 8 | SH1106 1.3" OLED (I2C) | 1 | $1.80 | $1.80 | AliExpress |
| 9 | Synchronous buck converter components | 1 | $4.00 | $4.00 | Assorted |
| 10 | IR2104 half-bridge driver | 1 | $1.00 | $1.00 | Mouser |
| 11 | N-channel MOSFETs (IRFP4468) | 2 | $2.50 | $5.00 | Mouser |
| 12 | Inductor 47µH 30A | 1 | $3.00 | $3.00 | Coilcraft |
| 13 | Capacitors (input/output) | 1 | $2.00 | $2.00 | Assorted |
| 14 | 12V fan + MOSFET driver | 1 | $2.50 | $2.50 | Assorted |
| 15 | MP1584 buck (5V/3A) | 1 | $1.00 | $1.00 | LCSC |
| 16 | AMS1117-3.3 LDO | 1 | $0.15 | $0.15 | LCSC |
| 17 | MC4 connectors (pair) | 1 | $0.50 | $0.50 | AliExpress |
| 18 | Heatsink (for MOSFETs) | 1 | $1.00 | $1.00 | AliExpress |
| 19 | 4-layer PCB (80×100mm) | 1 | $4.00 | $4.00 | JLCPCB |
| 20 | Enclosure (IP65, wall mount) | 1 | $5.00 | $5.00 | AliExpress |
| 21 | Micro USB connector | 1 | $0.20 | $0.20 | LCSC |
| 22 | Resistors/caps (assorted) | 1 | $1.00 | $1.00 | Assorted |
| | **TOTAL** | | | **$46.05** | |

**System Total BOM (typical 1-panel, 12-circuit home):**
- Hub: $27.15
- Circuit Monitor: $94.35
- Appliance Tags (8×): $103.60
- Solar Node: $46.05
- **Grand Total: ~$271.15** (retail components, no volume pricing)

---

## Block Diagrams

### System Power Flow
```
┌─────────────────────────────────────────────────────────┐
│                     ELECTRICAL PANEL                      │
│                                                           │
│  Mains ─┬─ Breaker 1 ──┬─── Circuit Monitor ──┐          │
│          │               │    (CT clamps)       │          │
│          ├─ Breaker 2 ───┤                      │          │
│          ├─ Breaker 3 ───┤                      │          │
│          ├─ ...          │                      │          │
│          ├─ Breaker 16 ──┘                      │          │
│          │                                     │          │
│          └──── Voltage Sensor ──────────────────┘          │
│                           │ Sub-GHz 868 MHz                │
│                           ▼                                │
│                    ┌──────────┐                           │
│                    │   HUB    │◄──── WiFi ──── Cloud       │
│                    │  NODE    │                            │
│                    └──────────┘                           │
│                    ▲         ▲                            │
│              BLE   │         │ Sub-GHz                    │
│                    │         │                            │
│          ┌─────────┘         └──────────┐                │
│          ▲                              ▲                │
│   ┌──────────────┐              ┌──────────────┐         │
│   │ Appliance    │              │   Solar      │         │
│   │ Tag ×8       │              │   Node       │         │
│   │ (BLE mesh)   │              │ (MPPT+BMS)  │         │
│   └──────────────┘              └──────────────┘         │
│         ▲                              ▲                 │
│         │                              │                 │
│    Wall outlets              Solar Panel + Battery        │
│    (fridge, TV, etc.)        (rooftop installation)       │
└─────────────────────────────────────────────────────────┘
```

### Data Flow Architecture
```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Circuit Monitor│────▶│    Hub Node     │────▶│     Cloud      │
│  (STM32G474)    │     │   (ESP32-S3)    │     │  (FastAPI +    │
│                 │     │                 │     │   TimescaleDB)  │
│  • 16× CT      │     │  • Aggregation  │     │                │
│  • Arc detect  │     │  • Local ML    │     │  • ML pipeline │
│  • RMS calc    │     │  • MQTT bridge │     │  • Forecasting │
│  • 4kHz sample │     │  • Alert logic │     │  • Billing     │
└─────────────────┘     │  • SD buffer   │     │  • Dashboard   │
                        └────────┬────────┘     └────────┬───────┘
                                 │                       │
                    ┌────────────┼───────────────┐       │
                    │            │               │       │
              ┌─────┴──┐   ┌────┴────┐   ┌─────┴──┐    │
              │Appliance│   │  Solar  │   │Mobile  │    │
              │  Tags   │   │  Node   │   │  App   │◄───┘
              │(nRF52)  │   │(RP2040) │   │(React) │
              └─────────┘   └─────────┘   └────────┘
```

---

## Safety & Compliance

### Electrical Safety
- **Circuit Monitor**: Installed inside breaker panel by qualified electrician. Full galvanic isolation between CT inputs and MCU (ISO7741 digital isolators). CT sensors are non-invasive (clamp-on, no wire cutting). AC-DC power supply (HLK-PM03) is UL-recognized with reinforced insulation.
- **Appliance Tags**: Double-insulated housing, no exposed mains connections. Solid-state relay with zero-cross switching to minimize inrush. Thermal fuse on AC path for overtemperature protection.
- **Solar Node**: MC4 connectors for PV input. All high-voltage sections are isolated. Emergency shutdown via hardware GPIO that forces buck converter duty cycle to 0%.

### EMC & Regulatory
- Sub-GHz radios: CE/FCC Part 15 compliant (868 MHz SRD band, <25 mW ERP)
- BLE: FCC Part 15 compliant
- All nodes: Designed for IEC 61010-1 (measurement equipment) compliance
- Arc fault detection: Designed to meet UL 1699 sensitivity requirements

---

## Getting Started

### Hardware Assembly
1. See `docs/assembly-guide.md` for step-by-step instructions
2. Circuit monitor must be installed by a licensed electrician
3. Appliance tags plug into any outlet — no wiring required
4. Solar node connects to your solar panel MC4 connectors
5. Hub node mounts near your breaker panel (within Sub-GHz range)

### Firmware Flashing
```bash
# Hub Node (ESP32-S3)
cd firmware/hub-node
idf.py build
idf.py flash

# Circuit Monitor (STM32G474)
cd firmware/circuit-monitor
make flash  # Uses OpenOCD + ST-Link

# Appliance Tag (nRF52840)
cd firmware/appliance-tag
west build -b nrf52840dongle_nrf52840
west flash

# Solar Node (RP2040)
cd firmware/solar-node
mkdir build && cd build
cmake .. && make
# Copy power_pulse_solar.uf2 to RP2040 USB mass storage
```

### Cloud Deployment
```bash
cd software/dashboard
docker-compose up -d  # Starts FastAPI + TimescaleDB + Mosquitto MQTT
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
power-pulse/
├── README.md                          # This file
├── schematic/
│   ├── hub-node/                      # KiCad project for Hub Node
│   │   ├── hub-node.kicad_sch
│   │   ├── hub-node.kicad_pcb
│   │   └── README.md
│   ├── circuit-monitor/               # KiCad project for Circuit Monitor
│   │   ├── circuit-monitor.kicad_sch
│   │   ├── circuit-monitor.kicad_pcb
│   │   └── README.md
│   ├── appliance-tag/                 # KiCad project for Appliance Tag
│   │   ├── appliance-tag.kicad_sch
│   │   ├── appliance-tag.kicad_pcb
│   │   └── README.md
│   └── solar-node/                    # KiCad project for Solar Node
│       ├── solar-node.kicad_sch
│       ├── solar-node.kicad_pcb
│       └── README.md
├── firmware/
│   ├── common/                        # Shared protocol & utilities
│   │   ├── powerpulse_protocol.h
│   │   ├── powerpulse_protocol.c
│   │   ├── crc16.c
│   │   └── README.md
│   ├── hub-node/                       # ESP32-S3 firmware (ESP-IDF)
│   │   ├── main/
│   │   │   ├── main.c
│   │   │   ├── wifi_task.c
│   │   │   ├── ble_mesh_task.c
│   │   │   ├── subghz_task.c
│   │   │   ├── cloud_task.c
│   │   │   ├── inference_task.c
│   │   │   ├── display_task.c
│   │   │   ├── alert_task.c
│   │   │   ├── config_task.c
│   │   │   ├── sd_task.c
│   │   │   │   └── include/
│   │   │       └── ... (headers)
│   │   ├── CMakeLists.txt
│   │   └── README.md
│   ├── circuit-monitor/                # STM32G474 firmware
│   │   ├── Src/
│   │   │   ├── main.c
│   │   │   ├── adc_task.c
│   │   │   ├── arc_detect_task.c
│   │   │   ├── power_calc_task.c
│   │   │   ├── subghz_task.c
│   │   │   ├── calibration_task.c
│   │   │   ├── temp_task.c
│   │   │   └── overload_detect.c
│   │   ├── Inc/
│   │   │   └── ... (headers)
│   │   ├── Makefile
│   │   └── README.md
│   ├── appliance-tag/                  # nRF52840 firmware
│   │   ├── src/
│   │   │   ├── main.c
│   │   │   ├── energy_task.c
│   │   │   ├── relay_task.c
│   │   │   ├── ble_mesh_task.c
│   │   │   ├── button_task.c
│   │   │   ├── display_task.c
│   │   │   └── calibration.c
│   │   ├── CMakeLists.txt
│   │   ├── prj.conf
│   │   └── README.md
│   └── solar-node/                    # RP2040 firmware
│       ├── src/
│       │   ├── main.c
│       │   ├── mppt_task.c
│       │   ├── bms_task.c
│       │   ├── solar_task.c
│       │   ├── load_task.c
│       │   ├── subghz_task.c
│       │   ├── thermal_task.c
│       │   ├── display_task.c
│       │   └── safety_task.c
│       ├── CMakeLists.txt
│       └── README.md
├── hardware/
│   └── bom/
│       ├── hub-node.csv
│       ├── circuit-monitor.csv
│       ├── appliance-tag.csv
│       └── solar-node.csv
├── software/
│   ├── dashboard/                     # FastAPI backend
│   │   ├── app/
│   │   │   ├── main.py
│   │   │   ├── config.py
│   │   │   ├── models.py
│   │   │   ├── routers/
│   │   │   │   ├── energy.py
│   │   │   │   ├── alerts.py
│   │   │   │   ├── devices.py
│   │   │   │   ├── solar.py
│   │   │   │   ├── automation.py
│   │   │   │   └── billing.py
│   │   │   ├── services/
│   │   │   │   ├── mqtt_handler.py
│   │   │   │   ├── anomaly_detector.py
│   │   │   │   ├── bill_estimator.py
│   │   │   │   ├── disaggregator.py
│   │   │   │   └── solar_optimizer.py
│   │   │   ├── ml/
│   │   │   │   ├── arc_model.py
│   │   │   │   ├── load_model.py
│   │   │   │   └── forecast_model.py
│   │   │   └── database.py
│   │   ├── requirements.txt
│   │   ├── Dockerfile
│   │   └── docker-compose.yml
│   ├── ml-pipeline/                    # ML training & export
│   │   ├── train_arc_detector.py
│   │   ├── train_appliance_nilm.py
│   │   ├── train_consumption_forecast.py
│   │   ├── train_anomaly_detector.py
│   │   ├── features/
│   │   │   ├── circuit_features.py
│   │   │   ├── appliance_features.py
│   │   │   └── time_features.py
│   │   ├── evaluate.py
│   │   ├── export_tflite.py
│   │   └── requirements.txt
│   └── mobile-app/                     # React Native
│       ├── App.tsx
│       ├── src/
│       │   ├── screens/
│       │   ├── components/
│       │   ├── services/
│       │   ├── hooks/
│       │   └── theme.ts
│       ├── package.json
│       ├── tsconfig.json
│       └── app.json
├── docs/
│   ├── architecture.md
│   ├── api-spec.md
│   ├── protocol-spec.md
│   ├── assembly-guide.md
│   └── calibration-guide.md
└── scripts/
    ├── deploy.sh
    ├── calibrate_ct.py
    ├── flash_all.sh
    └── simulate_data.py
```

---

## Key Innovation Points

1. **Per-Circuit Arc Fault Detection** — The first affordable, open-source residential arc fault detection system. Detects dangerous arcing on individual circuits using spectral analysis on CT data, long before a breaker trips.

2. **Sub-GHz for Panel Communication** — Circuit monitors are inside metal breaker panels where WiFi and BLE don't penetrate. Sub-GHz 868 MHz gets through where 2.4 GHz fails.

3. **BLE Mesh for Appliance Tags** — No WiFi setup required for each plug. Tags form a self-healing mesh, relay data back to the hub. Works even when your WiFi goes down.

4. **Hybrid NILM + Direct Measurement** — Combines per-circuit CT data with per-appliance BLE tag data for unprecedented accuracy. The ML pipeline can disaggregate untagged appliances too.

5. **Solar Self-Consumption Optimization** — Continuously adjusts load scheduling to maximize solar self-consumption: pre-heat water heater when solar is peak, delay EV charging to solar hours, shift dishwasher to midday.

6. **Offline-First Design** — Hub buffers data to microSD when internet is down. Local arc fault detection and overload alerts work without cloud. Appliance tags work without WiFi.

7. **Open, Affordable, Modular** — Full BOM under $300 for a typical home. Every schematic, every line of firmware, every ML model is open source. Add more CT channels, more appliance tags, or skip the solar node — it all works.

---

*PowerPulse — know every watt. Protect every circuit. Optimize every dollar.*