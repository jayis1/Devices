# BrewSync — AI-Powered Home Fermentation & Craft Brewing Intelligence

> **From grain to glass — never lose a batch again.**

BrewSync is a multi-node fermentation monitoring, prediction, and control system for home brewers, craft breweries, and fermentation enthusiasts (beer, wine, kombucha, mead, cheese, kimchi). It continuously monitors gravity, temperature, CO2 evolution, pressure, pH, and dissolved oxygen across multiple fermentation vessels, predicts stuck fermentations days in advance, auto-regulates temperature profiles, and provides brew-day guidance through a mobile app.

## Why BrewSync?

- **30% of homebrewers lose at least one batch per year** to stuck fermentations, temperature crashes, or infections
- Gravity tracking requires daily manual hydrometer readings — most people guess
- Temperature control is the #1 factor in beer quality, yet most fermenters sit in a closet
- No integrated system exists that combines real-time gravity, gas evolution, AND temperature control
- Fermentation science is data-poor — BrewSync generates the dataset that changes that

## System Architecture

```
┌─────────────┐     Sub-GHz 868 MHz      ┌──────────────┐
│  Fermenter  │◄──────────────────────────►│              │
│  Node ×N    │                            │              │
└─────────────┘                            │   BrewSync   │
┌─────────────┐     Sub-GHz 868 MHz      │    Hub       │
│  Cellar     │◄──────────────────────────►│  (RP2040 +  │
│  Monitor    │                            │   ESP32-C3)  │
└─────────────┘                            │              │
┌─────────────┐     BLE 5.0               │              │
│  Brew       │◄──────────────────────────►│              │
│  Scanner    │                            └──────┬───────┘
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

### 1. BrewSync Hub (Gateway)
- **SoC**: RP2040 (real-time protocol management) + ESP32-C3 (Wi-Fi/MQTT bridge)
- **Role**: Coordinates all nodes, runs local fermentation state machine, displays status on 3.5" IPS LCD, provides Wi-Fi uplink to cloud
- **Sensors**: Ambient temp/humidity (SHT40), barometric pressure (BMP390)
- **Actuators**: 2× relay for heating/cooling control per zone, buzzer for alerts
- **Comms**: Sub-GHz 868 MHz (SX1262) to Fermenter/Cellar nodes, BLE 5.0 to Brew Scanner, Wi-Fi to cloud
- **Power**: 5V USB-C, ~150mA avg

### 2. Fermenter Node (×N, one per vessel)
- **SoC**: STM32L476RG (ultra-low-power ARM Cortex-M4F)
- **Role**: Per-vessel fermentation monitoring — gravity, temp, CO2, pressure, pH
- **Sensors**:
  - Tilt-style SG gravity sensor (accelerometer-based, ADXL362)
  - DS18B20 waterproof temperature probe (±0.1°C)
  - SCD41 CO2 sensor (400–5000 ppm, ±40 ppm) for off-gas monitoring
  - MS5837 pressure sensor (0–30 bar, for pressurized fermentations)
  - Analog pH probe (EZO-pH embedded circuit, ±0.01 pH)
- **Comms**: Sub-GHz 868 MHz (SX1262) to Hub, battery-backed RTC for offline logging
- **Power**: 18650 Li-Ion + MCP73871 charger, solar optional, ~2mA avg (sleep between readings)
- **Enclosure**: IP65 rated, food-safe POM housing, magnetic mount to fermenter exterior

### 3. Cellar Monitor (per room/zone)
- **SoC**: STM32L476RG
- **Role**: Ambient environment monitoring for barrel aging, cellar conditioning, cold crash zones
- **Sensors**:
  - SHT40 temp/humidity (±0.1°C / ±1.5% RH)
  - BMP390 barometric pressure
  - LIS2DH12 vibration sensor (barrel movement / theft detection)
  - TSL2591 ambient light (UV exposure warning)
- **Comms**: Sub-GHz 868 MHz (SX1262) to Hub
- **Power**: 18650 Li-Ion, ~0.5mA avg
- **Enclosure**: IP67, wall-mount

### 4. Brew Scanner (handheld)
- **SoC**: ESP32-S3 (dual-core, ML-capable)
- **Role**: Brew-day and sampling tool — measures OG/FG with refractometer, identifies hop/yeast varieties, checks for infection
- **Sensors**:
  - AS7341 11-channel spectral sensor (340–880nm) for beer color, IBU estimation, infection spectral signature
  - VL53L1X ToF distance sensor (meniscus detection for volume measurement)
  - SCD41 CO2 (fermentation vigor of sample)
  - ICM-42670 IMU (shake detection for degassing samples)
- **Comms**: BLE 5.0 to Hub, Wi-Fi direct for OTA updates
- **Power**: 1000mAh Li-Po, USB-C charging, ~8hr continuous use
- **Display**: 1.3" 240×240 IPS LCD for instant readings

## Communication Protocol

All nodes use the **BrewSync Mesh Protocol (BSMP)** over Sub-GHz 868 MHz:
- **Frequency**: 868.0 MHz (EU) / 915 MHz (US), LoRa-like modulation via SX1262
- **Modulation**: LoRa SF7–SF12 (adaptive), 125 kHz BW
- **Topology**: Star (Hub-centric), with Hub as coordinator
- **Frame format**: `[PREAMBLE(4)] [ADDR(2)] [SEQ(1)] [TYPE(1)] [LEN(1)] [PAYLOAD(0-200)] [CRC(2)]`
- **Encryption**: AES-128-CCM (shared key provisioned at pairing)
- **Duty cycle**: Fermenter nodes report every 5 min (normal) / 30 sec (active fermentation), Cellar every 10 min
- **BLE Scanner**: GATT-based, 100 ms advertising interval when paired

## Firmware

Each node runs bare-metal C firmware on STM32L4 / RP2040 / ESP32:
- **Common**: `common/protocol.c` (BSMP frame encode/decode), `common/aes128.c` (encryption), `common/sensors.c` (shared drivers)
- **Fermenter**: `fermenter/main.c`, `fermenter/gravity.c` (tilt accelerometer → SG), `fermenter/co2.c`, `fermenter/ph.c`
- **Cellar**: `cellar/main.c`, `cellar/environment.c`
- **Scanner**: `scanner/main.c`, `scanner/spectral.c`, `scanner/display.c`
- **Hub**: `hub/main.c`, `hub/coordinator.c`, `hub/lcd.c`, `hub/wifi_bridge.c`

## Cloud / Edge Software

### FastAPI Dashboard
- REST + WebSocket API for real-time fermentation data
- PostgreSQL timeseries (with TimescaleDB extension)
- Per-batch tracking: recipe import (BeerXML), fermentation timeline, gravity/temperature curves
- Alert engine: stuck fermentation, temperature excursions, infection probability, target FG reached
- Multi-batch comparison, historical analysis
- Recipe suggestions based on yeast strain + temperature profile

### ML Pipeline
1. **Fermentation Progress Model** — LSTM predicting final gravity and completion time from first 24h of data
2. **Stuck Fermentation Predictor** — Random Forest on CO2 evolution rate, temperature gradient, pH trend; 72-hour advance warning
3. **Infection Detector** — Spectral anomaly detection on Brew Scanner data; 15-class common contaminant identification
4. **Yeast Health Model** — Estimates cell count and viability from CO2 evolution curve and temperature
5. **Recipe Optimizer** — Recommends temperature schedule and yeast pitch rate based on style guidelines and historical data
6. **Flavor Predictor** — Predicts IBU, ABV, color (SRM), and flavor profile from grain bill + fermentation data

## Mobile App (React Native)

- **Dashboard**: Real-time fermentation status for all active vessels
- **Brew Day**: Step-by-step guided brew day with timers, temperature alerts, gravity logging
- **Scanner**: Connect to Brew Scanner for instant OG/FG readings, infection checks
- **History**: Browse past batches, compare fermentations, share recipe cards
- **Alerts**: Push notifications for temperature excursions, stuck fermentations, target gravity reached
- **Recipes**: Import BeerXML, scale recipes, get style-compliant suggestions
- **Community**: Anonymized fermentation data sharing (opt-in)

## Bill of Materials (Per Node)

See `hardware/bom/` for detailed CSVs with part numbers, quantities, and pricing.
- **Hub**: ~$38 (RP2040 + ESP32-C3 + SX1262 + LCD + relays + sensors)
- **Fermenter Node**: ~$45 (STM32L476 + sensors + battery + IP65 enclosure)
- **Cellar Monitor**: ~$22 (STM32L476 + sensors + battery + IP67 enclosure)
- **Brew Scanner**: ~$52 (ESP32-S3 + spectral + display + battery)

## Key Innovations

1. **Tilt SG sensor + CO2 evolution = dual-confirmation fermentation tracking** — no more "is it done?" guessing
2. **72-hour stuck fermentation prediction** — catch problems before they ruin your batch
3. **Spectral infection detection** — catch lactobacillus, acetobacter, wild yeast early
4. **Automated temperature control** — relay-driven heating/cooling per fermentation schedule
5. **Offline-capable** — Fermenter nodes log internally when Hub is down, sync when reconnected
6. **BeerXML recipe import** — works with existing recipe software (Brewfather, Brewer's Friend)

## License

MIT — brew it, sell it, improve it.

---

*Invented and maintained by [jayis1](https://github.com/jayis1). Part of the [Devices](https://github.com/jayis1/Devices) collection.*