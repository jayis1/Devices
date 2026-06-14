# UrbanHarvest

**Intelligent urban micro-farming system.** Turns balconies, rooftops, windowsills, and spare closets into productive food gardens — automatically. Monitors every plant, waters when needed, doses nutrients, adjusts lighting, detects disease early, and predicts your harvest.

---

## What It Does

UrbanHarvest is a 4-node system that makes growing food at home effortless, reliable, and productive:

1. **Senses** every plant's environment — soil moisture, EC (nutrient level), pH, temperature, light (PAR), leaf wetness — distributed across all your planters and beds
2. **Acts** automatically — controls irrigation pumps, nutrient dosing peristaltic pumps, LED grow lights (spectrum + intensity), ventilation fans, heaters, and humidifiers in grow pods
3. **Sees** your plants — built-in camera in grow pods takes daily photos, runs on-device ML for disease and pest detection before you'd ever notice by eye
4. **Predicts** harvest dates, yield, and nutrient depletion using time-series ML — tells you "Cherry tomatoes ready in ~9 days, expected 2.3 kg"
5. **Adapts** to outdoor conditions — weather station node tracks rain, wind, UV, and adjusts outdoor planter watering (skip if rain is coming, shade if UV is extreme)
6. **Guides** you — mobile app shows what to plant now for your climate, when to transplant, how to prune, with personalized ML-driven recommendations
7. **Connects** your kitchen — recipe suggestions based on what's harvest-ready, food waste tracking, seasonal planning

All plant sensors communicate over a dedicated Sub-GHz mesh network (reliable through walls and soil, no WiFi dependency for plant-critical irrigation). The hub bridges to WiFi for cloud analytics and the mobile app. The grow pod uses both Sub-GHz (to the hub) and local GPIO (direct actuator control). The weather station uses Sub-GHz to report conditions.

### The Problem It Solves

- 55% of the world's population lives in cities; most have zero food production capability
- 1.3 billion tons of food are wasted annually — much from over-buying and spoilage (FAO)
- Home gardening has a 70% failure rate for beginners — wrong watering, wrong light, wrong nutrients, pests caught too late
- A single forgotten watering can kill a month of growth; overwatering causes root rot just as fast
- Most people don't know what grows in their climate, when to plant, or how to troubleshoot problems
- Store-bought herbs and microgreens are 5-10x the cost of home-grown and lose 40% of nutrients in transit
- Urban food deserts affect 23.5 million Americans with limited access to fresh produce
- Climate anxiety: people want to do something tangible — growing even a fraction of their food reduces carbon footprint, packaging, and transport emissions
- The "black thumb" problem: plants die silently — by the time you see yellow leaves, the problem started days ago

UrbanHarvest makes it so your plants basically grow themselves. It's the difference between guessing and knowing. The system catches problems early, automates the tedious daily care, and teaches you to be a better gardener over time. Even experienced gardeners benefit from the precision monitoring and disease early-warning — catching powdery mildew on day 1 instead of day 5 can save an entire crop.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         URBANHARVEST SYSTEM                                        │
│                                                                                    │
│  ┌──────────────────┐  Sub-GHz   ┌────────────────────────────┐                   │
│  │ PLANT SENSOR #1  │◄──────────►│                            │                   │
│  │ (Basil pot)      │  868MHz   │                            │                   │
│  │ Soil moisture    │  mesh     │                            │                   │
│  │ Soil EC          │           │       HUB NODE             │                   │
│  │ Soil temp        │           │   (nRF5340 + ESP32-C6)     │──── WiFi6 ──► Cloud
│  │ Light (PAR)      │           │                            │               Dashboard
│  │ Leaf wetness     │           │   3.2" TFT                │               + ML Pipeline
│  └──────────────────┘           │   Garden dashboard        │               + Plant ID
│                                  │   Voice interface         │               + Harvest
│  ┌──────────────────┐           │   TFLite Micro             │─── BLE ───► Mobile
│  │ PLANT SENSOR #2  │◄─────────►│   Plant health classifier  │               App
│  │ (Tomato bed)     │  Sub-GHz  │   Disease detector         │               (Setup,
│  │ Soil moisture    │  mesh     │                            │                Alerts,
│  │ Soil EC/pH       │           │                            │                Harvest)
│  │ Soil temp        │           └──────────┬─────────────────┘
│  │ Light (PAR)      │                      │
│  │ Leaf wetness     │                      │ Sub-GHz mesh
│  └──────────────────┘                      │ (up to 24 plant sensors
│                                            │  per hub)
│  ┌──────────────────┐                      │
│  │ PLANT SENSOR #N  │◄─────────────────────┘
│  │ (any planter)    │   Sub-GHz
│  └──────────────────┘   mesh
│
│  ┌──────────────────────────┐            ┌──────────────────────┐
│  │    GROW POD CONTROLLER   │◄──────────►│   WEATHER STATION    │
│  │    (ESP32-S3 + cam)      │  Sub-GHz   │   (RP2040)          │
│  │    LED grow lights       │  mesh      │   Rain gauge         │
│  │    Water pump            │            │   Anemometer          │
│  │    Nutrient dosing       │            │   UV/Temp/RH/Pressure│
│  │    Fan/Humidifier/Heat   │            │   Solar powered      │
│  │    OV2640 camera         │            │   5W panel + Lipo    │
│  │    Disease detection ML │            │                      │
│  └──────────────────────────┘            └──────────────────────┘
│
│  ┌──────────────────────────────────────────────────────────────────────────────┐
│  │                      CLOUD / EDGE SOFTWARE                                    │
│  │  ┌──────────────┐  ┌──────────────────┐  ┌───────────────────────────────┐  │
│  │  │  Dashboard   │  │  ML Pipeline     │  │  Mobile App                   │  │
│  │  │  (React)     │  │  Disease detect  │  │  (React Native)               │  │
│  │  │  Garden map  │  │  Yield predict   │  │  Plant health cards            │  │
│  │  │  Harvest log │  │  Irrigation opt  │  │  Harvest countdown            │  │
│  │  │  Analytics   │  │  Nutrient advis  │  │  Recipe suggestions           │  │
│  │  └──────────────┘  └──────────────────┘  └───────────────────────────────┘  │
│  └──────────────────────────────────────────────────────────────────────────────┘
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per home)

The brain. Bridges Sub-GHz mesh to WiFi/BLE/cloud. Runs local ML models for plant health assessment and disease detection. Displays a live garden dashboard.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF5340 (dual Cortex-M33) | Application core runs mesh + ML; network core runs BLE 5.0 |
| WiFi Bridge | ESP32-C6-MINI-1 | WiFi 6 + BLE bridge to cloud |
| Radio | SX1262 (868MHz) | Sub-GHz LoRa mesh coordinator to all nodes |
| Display | 3.2" IPS TFT (ILI9341) | Garden dashboard: plant health map, growth timeline, alerts |
| Audio Out | MAX98357A (I2S amp) + 3W speaker | Voice alerts: "Tomato plant 2 needs water", "Mildew detected on basil" |
| Audio In | SPH0645LM4H (I2S MEMS mic) | Voice commands: "How are my plants?", "When is harvest ready?" |
| On-board Sensors | BME688 (env), TSL25911 (light) | Hub's own ambient monitoring (room temp/humidity for indoor garden context) |
| Storage | 16MB W25Q128 Flash + SD card | Plant image cache, OTA updates, ML model storage, growth timelapses |
| LEDs | RGB status LED + 4× zone LEDs | Visual health indicators (green=thriving, yellow=attention, red=critical) |
| Emergency | Buzzer (piezo 3V) | Loud alarm for critical events (pump failure, leak detected) |
| Power | 5V USB-C + Lipo 3000mAh | Stays running during power outage (8+ hours, plants still get watered) |
| Connectors | 2× I2C, 1× UART, 1× SPI, 8× GPIO | Expansion (future: CO2 sensor for greenhouse, additional actuators) |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler, assigns slots to all nodes)
- Aggregates soil and environmental data from all plant sensors
- Receives grow pod camera images, runs TFLite Micro disease classifier (5-class: healthy, powdery mildew, downy mildew, leaf spot, nutrient deficiency)
- Runs TFLite Micro plant health index calculator (soil moisture + EC + temp + light → health score 0-100)
- WiFi uplink to MQTT broker (QoS 1, TLS) for cloud analytics
- BLE GATT server for mobile app pairing and configuration
- TFT dashboard rendering (plant health cards, growth curves, watering schedule, harvest countdown)
- Voice alerts via speaker for critical events (disease detected, soil critically dry, pump failure)
- Two-way voice for configuration and queries ("How's the basil doing?", "Water the tomatoes now")
- OTA update distribution to all nodes
- Local automation continues when WiFi is down (plants still get watered on schedule)

### 2. Grow Pod Controller (1-4 per home)

Installed in or near a grow shelf, grow tent, or indoor garden area. This is the "actuator" node that makes the system close the loop — not just sensing, but growing. Controls the full indoor growing environment: lights, water, nutrients, climate.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3 (dual Xtensa LX7) | 240MHz, 8MB PSRAM, WiFi, camera interface |
| Camera | OV2640 (2MP) | Daily plant photos for disease detection, growth tracking, timelapse |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client to hub |
| LED Driver | 4× AL8860 (constant current buck) | Drives 4 channels of LED grow lights: Deep Red (660nm), Royal Blue (450nm), Warm White (3000K), Far Red (730nm) |
| LED Array | Custom 4-channel LED strip | Full spectrum grow lighting — dimmable per channel for spectrum control |
| Water Pump | 12V DC submersible pump + MOSFET (IRLZ44N) | Automated irrigation via drip lines |
| Nutrient A | Stepper-driven peristaltic pump (28BYJ-48 + ULN2003) | Doses nutrient solution A (e.g., FloraGro) |
| Nutrient B | Stepper-driven peristaltic pump (28BYJ-48 + ULN2003) | Doses nutrient solution B (e.g., FloraBloom) |
| pH Dosing | Micro peristaltic pump (0622A + DRV8833) | pH Up/Down solution dosing |
| Fan | 12V DC fan + PWM control (MOSFET) | Ventilation — temperature and humidity regulation |
| Heater | SSR-25DA + heat mat (15W) | Temperature control for root zone and air |
| Humidifier | Ultrasonic mist maker + MOSFET | Humidity control |
| Flow Sensor | YF-S201 water flow sensor | Measures irrigation volume — tracks water usage, detects leaks |
| Environment | BME688 (I2C) | Temperature, humidity, pressure, VOC (detects off-gassing from plastics/fertilizers) |
| Light | TSL25911 (I2C) | PAR (photosynthetically active radiation) sensor — verifies LED output |
| Water Temp | DS18B20 (waterproof, OneWire) | Nutrient solution temperature (critical for hydroponics: 18-24°C optimal) |
| Relays | 4× OMRON G5LE-14 5V | Dry contact: main pump, exhaust fan, circulation fan, heater backup |
| Power | 24V DC barrel jack (for LEDs) + USB-C 5V | LED drivers need 24V; MCU + sensors on 5V |
| Enclosure | IP54, DIN rail or shelf-mount 120×80×40mm | Near grow area, moisture-resistant |

**Grow pod firmware responsibilities:**
- Receives irrigation commands from hub via mesh: "Water plant #3, 150ml"
- Camera capture: daily photo of each plant zone at scheduled times, sent to hub for ML analysis
- On-board TFLite Micro disease classifier processes camera images (5-class model, 120×120 input)
- LED light schedule: configurable photoperiod (e.g., 16h on / 8h off for vegetative, 12/12 for flowering)
- LED spectrum control: adjusts red/blue/white/far-red ratios for growth stage (seedling → vegetative → flowering)
- Nutrient dosing: peristaltic pumps dose A and B solutions based on EC readings from plant sensors
- pH management: monitors pH from plant sensor data, doses pH up/down to maintain 5.8-6.2 (hydroponic) or 6.0-7.0 (soil)
- Climate control: fan speed, heater, humidifier based on BME688 readings
- Flow sensor: tracks total water delivered per irrigation, detects leaks (flow when pump off)
- Safety interlocks: never run pump when flow sensor shows no flow (dry pump protection), never heat above 35°C, never humidify above 90% RH
- Water temperature monitoring: alerts if nutrient solution is too warm (>26°C) or too cold (<15°C)
- TFLite Micro light level verifier: compares TSL25911 PAR reading to expected LED output, detects LED degradation

### 3. Plant Sensor Node (1-24 per home)

Pushed into the soil of each planter, pot, raised bed, or balcony container. The core sensing node. Measures everything happening underground and above that determines plant health.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32WL55CC | Dual-core (Cortex-M4 + M0+) with integrated Sub-GHz radio, 256KB Flash, 64KB RAM |
| Soil Moisture | Capacitive soil moisture sensor v1.2 (analog) | Volumetric water content — corrosion-resistant, lasts years in soil |
| Soil EC | Stainless steel electrodes (custom PCB trace) | Electrical conductivity → nutrient concentration in soil solution |
| Soil Temperature | DS18B20 (waterproof, OneWire) | Root zone temperature — critical for germination and root health |
| Light (PAR) | TSL25911 (I2C) | Photosynthetically active radiation (400-700nm) — is the plant getting enough light? |
| Leaf Wetness | Custom capacitive sensor (interdigitated PCB trace) | Surface wetness on leaf mimic — detects dew, rain splash, overhead watering; early warning for fungal disease |
| Radio | Built-in Sub-GHz (868MHz, STM32WL integrated) | Mesh client — no external radio chip needed |
| Antenna | 868MHz whip antenna (50mm) | Through-PCB or external whip for soil-penetrating range |
| Power | 3× AA alkaline (4.5V) or optional 2W solar panel + Lipo 600mAh | Battery life: 6+ months on AA; solar variant is perpetual |
| Enclosure | IP68 waterproof, 35×120×15mm probe | Pushed into soil, only top cap exposed |
| LED | Single RGB LED (WS2812B mini) | Visual status: green (healthy), yellow (attention needed), red (critical) |
| Button | 12mm tactile (waterproof boot) | Pairing + manual read trigger |

**Plant sensor firmware responsibilities:**
- Capacitive soil moisture reading every 5 minutes (ADC, 12-bit, oversampled 16x)
- Soil EC measurement every 15 minutes (AC excitation, 4-wire measurement to avoid electrode polarization)
- DS18B20 soil temperature every 15 minutes
- TSL25911 PAR light level every 10 minutes
- Leaf wetness every 5 minutes (critical for disease risk — wetness duration >6h = high fungal risk)
- All readings packetized and sent to hub via mesh TDMA slot
- On-board TFLite Micro soil health classifier (moisture + EC + temp → soil health index 0-100)
- Alert flags: if moisture < 15% or > 85%, or EC > 3.0 mS/cm (nutrient burn), flag as URGENT
- Self-calibration: soil moisture sensor auto-zeros to air (dry = 0%) on first boot
- Ultra-low-power idle between readings (~50µA with RTC running)
- RGB LED blinks once per hour showing health status (green/yellow/red)
- Average current: ~300µA (giving 6+ months on 3× AA alkaline)

### 4. Weather Station Node (1 per home, optional)

Mounted outdoors (balcony, rooftop, garden wall). Monitors the conditions that affect outdoor and balcony plants. Feeds data to the irrigation optimizer so it can skip watering when rain is coming, or increase watering during heatwaves.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 (dual Cortex-M0+) | 133MHz, 264KB SRAM, low cost, plenty of GPIO for weather sensors |
| Radio | SX1262 (868MHz) | Sub-GHz mesh client to hub |
| Wind Speed | Anemometer (reed switch, cup type) | Wind speed via GPIO interrupt — wind dries soil faster, affects transpiration |
| Wind Direction | Potentiometer vane (ADC) | Wind direction — for shelter-in-place decisions |
| Rain | Tipping bucket rain gauge (reed switch) | Rainfall volume — skip irrigation after rain, track cumulative precipitation |
| Temperature/Humidity | Sensirion SHT45 (I2C) | High-accuracy outdoor temp and RH (±0.2°C, ±1.8% RH) |
| Barometric Pressure | BMP390 (I2C) | Atmospheric pressure — barometric trend for weather prediction |
| UV Index | VEML6075 (I2C) | UV-A and UV-B intensity — sunburn risk for sensitive plants |
| Light | TSL25911 (I2C) | Full-spectrum ambient light — outdoor PAR estimation, day length |
| Solar | 5W 6V polycrystalline panel | Powers the station + charges battery |
| Battery | Lipo 2000mAh + MCP73871 charger | Runs 72+ hours without sun; solar keeps it charged year-round |
| Power Management | BQ25570 (energy harvester) | MPPT solar charge + buck regulator — extracts maximum power from small panel |
| Enclosure | IP65 Stevenson screen style, 150×100×80mm | UV-stable ASA plastic, louvered for airflow, shields sensors from direct rain/sun |
| Mounting | Pole mount + U-bolt kit | Balcony railing, fence post, or wall mount |

**Weather station firmware responsibilities:**
- SHT45 temperature/humidity every 60 seconds
- BMP390 pressure every 60 seconds
- VEML6075 UV every 60 seconds
- TSL25911 light every 60 seconds
- Anemometer: wind speed accumulated over 10-second intervals, reported every 60 seconds (m/s)
- Wind vane: direction reported every 60 seconds (8-point compass)
- Rain gauge: tip count accumulated, reported every 60 seconds (each tip = 0.2794mm rain)
- All readings packetized and sent to hub via mesh TDMA slot
- On-board barometric trend calculation (3-hour pressure delta → weather forecast: rising/falling/steady)
- Rain prediction: falling pressure > 2hPa/3h + humidity > 80% → "Rain likely in 2-6 hours"
- Solar power management: reports panel voltage and battery SOC to hub
- Ultra-low-power at night: sensors sleep, only RTC and radio wake for scheduled TX
- Safety: if wind > 50 km/h, flag URGENT (plant damage risk — "Move potted plants to shelter")

---

## Communication Protocol

### Sub-GHz Mesh (SX1262/SX1261/STM32WL, 868MHz LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa SF7 (normal) / SF10 (long range for weather station) |
| Bandwidth | 125 kHz |
| TX Power | +14 dBm (EU) / +20 dBm (US) |
| Range | 30m indoor (normal) / 200m outdoor (long range) |
| Protocol | Custom TDMA (hub is coordinator) |
| Slot Duration | 100ms per node |
| Cycle Time | 2.5 seconds (24 data slots + 1 control) |

### TDMA Frame Structure

```
| SLOT 0 (HUB) | SLOT 1 (PS1) | SLOT 2 (PS2) | ... | SLOT 12 (PS12) | SLOT 13 (GP) | SLOT 14 (WS) | S15-S24 | SLOT 25 (CTRL) |
|    100ms     |    100ms     |    100ms     |     |    100ms       |    100ms     |    100ms     |  100ms  |    100ms       |

Total frame: 2600ms (26 slots × 100ms)
Slot 0: Hub broadcasts sync + commands + acknowledgment
Slots 1-12: Plant sensors 1-12 uplink soil/environmental data
Slot 13: Grow pod uplink status + camera availability flag
Slot 14: Weather station uplink outdoor conditions
Slots 15-24: Plant sensors 13-24 (if present)
Slot 25: Control/ACK/retransmit/alert broadcast

ALERT OVERRIDE: Critical soil conditions trigger immediate transmission
on slot 25 regardless of TDMA schedule (CSMA fallback).
Camera images are NOT sent over Sub-GHz — they go via WiFi from grow pod to cloud,
or via UART to hub for on-device ML processing.
```

### Mesh Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | SEQ(2) | PAYLOAD(0-48) | CRC16(2) ]

TYPE values:
  0x01 = SOIL_DATA      (moisture%, EC mS/cm, temp°C, pH, health_index)
  0x02 = LIGHT_DATA     (PAR µmol/m²/s, lux, UV_index)
  0x03 = LEAF_WETNESS   (wetness%, duration_h, dew_point)
  0x04 = GROW_POD_STATUS (pump_state, nutrient_A_ml, nutrient_B_ml, pH_dose_ml, fan_speed, heater_state, light_schedule)
  0x05 = WEATHER_DATA    (temp, RH, pressure, wind_speed, wind_dir, rain_mm, UV, light_lux)
  0x06 = IRRIGATION_CMD  (hub → grow_pod: plant_id, volume_ml, duration_s)
  0x07 = NUTRIENT_CMD    (hub → grow_pod: plant_id, nutrient_A_ml, nutrient_B_ml, pH_adj_ml)
  0x08 = LIGHT_CMD       (hub → grow_pod: red_pwm, blue_pwm, white_pwm, far_red_pwm, on/off)
  0x09 = DISEASE_ALERT   (plant_id, disease_class, confidence, image_id for cloud)
  0x0A = ACK             (acknowledgment)
  0x0B = OTA_BLOCK       (firmware update chunk)
  0x0C = HARVEST_PREDICT (plant_id, expected_date, estimated_yield_g)
  0x0D = HEARTBEAT       (periodic alive signal)
  0x0E = CALIBRATION     (sensor calibration data)
  0x0F = DANGER_ALERT    (CRITICAL — immediate transmission, bypasses TDMA)
  0x10 = CAMERA_READY    (grow_pod → hub: image captured, ready for retrieval via WiFi)
```

### BLE Protocol (Mobile App ↔ Hub)

```
GATT Service: UrbanHarvest (0xUH01)
  Characteristic 0xUH11: Garden Summary (notify, 8 bytes: num_plants, avg_health, alerts_count, harvest_ready)
  Characteristic 0xUH12: Plant Detail (read, 16 bytes: plant_id, moisture, EC, temp, light, health, disease_flag)
  Characteristic 0xUH13: Irrigation Control (write, 4 bytes: plant_id, volume_ml, immediate_flag)
  Characteristic 0xUH14: Light Control (write, 5 bytes: pod_id, red, blue, white, far_red)
  Characteristic 0xUH15: Alert Config (write, 8 bytes: moisture_low, moisture_high, ec_max, temp_min, temp_max)
  Characteristic 0xUH16: Weather (notify, 12 bytes: temp, RH, pressure, wind, rain, UV)
  Characteristic 0xUH17: Voice Command (write, variable: UTF-8 text command to hub)

Advertising Packet (every 500ms):
  [ Flags(3) | Complete Local Name("UH-HUB-XXXX") | Manufacturer Data:
    [ CompanyID(0xUH01) | HubID(2) | NumPlants(1) | AvgHealth(1) | Alerts(1) | HarvestReady(1) ] ]
```

---

## AI / ML Pipeline

### 1. Plant Health Index (on Plant Sensor, TFLite Micro)

- **Input**: Soil moisture (%), soil EC (mS/cm), soil temperature (°C), light (PAR), leaf wetness duration (hours) — 5 features
- **Model**: TinyML 3-layer fully connected network (5→16→8→1), INT8 quantized, 4KB
- **Output**: Plant health index (0-100: 0=dead, 50=stressed, 100=thriving)
- **Purpose**: Real-time plant health at the edge, no cloud dependency
- **Runs**: Every 15 minutes on each plant sensor
- **Accuracy**: ±8 health index points vs. expert horticulturist assessment
- **Calibration**: Per-plant-type (tomato health ≠ basil health at same moisture/EC)

### 2. Disease Detection (on Grow Pod + Hub, TFLite Micro)

- **Input**: 120×120 RGB image from OV2640 camera, 3-channel
- **Model**: MobileNetV2 backbone + custom classifier head, INT8 quantized, 280KB
- **Output**: 6-class classification: healthy, powdery mildew, downy mildew, leaf spot (bacterial), leaf spot (fungal), nutrient deficiency
- **Confidence threshold**: >0.70 triggers alert; >0.90 triggers urgent alert
- **Runs**: Daily per plant zone after morning light-on (best imaging conditions)
- **Training data**: 50,000+ labeled plant disease images (PlantVillage dataset + custom augmentations)
- **On-device**: Runs on ESP32-S3 (grow pod) with 8MB PSRAM; images also uploaded to cloud for continuous model improvement

### 3. Yield Prediction (Cloud, PyTorch)

- **Input**: Plant type, days since planting, cumulative light hours, cumulative water volume, average soil EC, temperature history, nutrient doses, disease events
- **Model**: LSTM sequence model with attention over 90-day growing season windows
- **Output**: Predicted harvest date (±3 days), estimated yield in grams
- **Personalization**: Transfer learning from generic crop models, fine-tuned on 21+ days of your specific plant data
- **Runs**: Daily, updates prediction as season progresses
- **Purpose**: "Your cherry tomatoes will be ready to harvest around July 18th, expecting ~2.3 kg"
- **Confidence**: Improves as season progresses (±14 days early season, ±3 days near harvest)

### 4. Irrigation Optimizer (Cloud, reinforcement learning)

- **Input**: All plant sensor readings, weather forecast (rain, temp, wind, humidity), plant water models (transpiration estimation), soil type, pot size
- **Model**: Deep Q-Network (DQN) — state = (moisture, weather, plant_type, days_since_watering), action = (water_volume_ml, wait)
- **Output**: Optimal irrigation schedule and volume per plant
- **Runs**: Every 30 minutes, sends updated schedule to hub → grow pod
- **Optimizes**: Minimize water waste while keeping soil moisture in target range (40-65% for most vegetables)
- **Rain integration**: If weather station reports rain >5mm in last 2h or forecast >3mm in next 6h, skip outdoor irrigation
- **Safety**: Never lets moisture drop below plant-specific critical threshold (e.g., 15% for tomatoes, 20% for lettuce)

### 5. Nutrient Advisor (Cloud, gradient-boosted trees)

- **Input**: Soil EC trend, plant growth stage, plant type, days since last feeding, water volume since last feeding, pH readings
- **Model**: XGBoost model with 14 features
- **Output**: Recommended nutrient dose (A and B solutions, in ml), pH adjustment if needed
- **Runs**: Daily at 6 AM, sends recommendations to hub → grow pod for automatic dosing
- **Safety constraints**: Never exceed EC > 2.5 mS/cm (nutrient burn risk), never adjust pH by >0.5 in one dose
- **Plant-specific**: Different nutrient profiles for vegetative vs. flowering vs. fruiting stages

### 6. Seasonal Planting Advisor (Cloud, rule-based + ML)

- **Input**: Your location (lat/lon), local frost dates (from weather API), your available grow space, your plant history, your preferences
- **Model**: Collaborative filtering + climate zone rules + success probability model
- **Output**: "Plant now" recommendations: what to plant this month for your climate, expected success rate, companion planting suggestions
- **Runs**: Weekly, pushes notifications to mobile app
- **Integration**: Feeds into irrigation optimizer (pre-configures optimal watering for new plant type)

---

## Pin Assignments

### Hub Node (nRF5340 + ESP32-C6)

**nRF5340 (application core + mesh coordination):**

| Pin | Function | Connected To |
|-----|----------|--------------|
| P0.00/P0.01 | UART0 TX/RX | ESP32-C6 UART1 (inter-MCU link) |
| P0.02/P0.03 | I2C0 SDA/SCL | BME688 (environment) + TSL25911 (light) |
| P0.04/P0.05 | I2C1 SDA/SCL | (expansion port: future CO2, pH, etc.) |
| P0.06 | SPI0 SCK | Flash + SD card |
| P0.07 | SPI0 MOSI | Flash + SD card |
| P0.08 | SPI0 MISO | Flash + SD card |
| P0.09 | SPI0 CS0 | Flash CS |
| P0.10 | SPI0 CS1 | SD card CS |
| P0.11 | SPI1 SCK | TFT display |
| P0.12 | SPI1 MOSI | TFT display |
| P0.13 | SPI1 MISO | TFT display (unused) |
| P0.14 | SPI1 CS | TFT CS |
| P0.15 | TFT_DC | Display data/command |
| P0.16 | TFT_RESET | Display reset |
| P0.17 | TFT_BL | Display backlight PWM |
| P0.18 | SX1262_BUSY | Radio busy signal |
| P0.19 | SX1262_IRQ | Radio interrupt |
| P0.20 | SX1262_NRST | Radio reset |
| P0.21 | SX1262_NSS | Radio SPI chip select |
| P0.22 | I2S_CLK | Audio codec clock (BCLK) |
| P0.23 | I2S_WS | Audio codec word select (LRCLK) |
| P0.24 | I2S_DOUT | MAX98357A (speaker DAC) |
| P0.25 | I2S_DIN | SPH0645LM4H (microphone) |
| P0.26 | BUZZER | Piezo buzzer (PWM, critical alerts) |
| P0.27 | LED_R | RGB status LED red |
| P0.28 | LED_G | RGB status LED green |
| P0.29 | LED_B | RGB status LED blue |
| P0.30/P0.31 | ZONE1/ZONE2 | Garden zone indicator LEDs |
| P1.00/P1.01 | ZONE3/ZONE4 | Garden zone indicator LEDs |

**ESP32-C6 (WiFi/BLE bridge):**

| Pin | Function | Connected To |
|-----|----------|--------------|
| GPIO4/GPIO5 | UART1 TX/RX | nRF5340 UART0 |
| GPIO12/GPIO13 | USB D+/D- | USB-C port |
| GPIO6-11 | SPI | Flash (internal) |
| GPIO0/GPIO1 | I2C SDA/SCL | (expansion port) |

### Grow Pod Controller (ESP32-S3 + Camera)

| Pin | Function | Connected To |
|-----|----------|--------------|
| GPIO1/GPIO2 | UART0 TX/RX | Debug/console |
| GPIO3 | CAM_D0 | OV2640 camera data bit 0 |
| GPIO4 | CAM_D1 | OV2640 camera data bit 1 |
| GPIO5 | CAM_D2 | OV2640 camera data bit 2 |
| GPIO6 | CAM_D3 | OV2640 camera data bit 3 |
| GPIO7 | CAM_D4 | OV2640 camera data bit 4 |
| GPIO8 | CAM_D5 | OV2640 camera data bit 5 |
| GPIO9 | CAM_D6 | OV2640 camera data bit 6 |
| GPIO10 | CAM_D7 | OV2640 camera data bit 7 |
| GPIO11 | CAM_VSYNC | OV2640 vertical sync |
| GPIO12 | CAM_HREF | OV2640 horizontal reference |
| GPIO13 | CAM_PCLK | OV2640 pixel clock |
| GPIO14 | CAM_XCLK | OV2640 system clock (20MHz, driven by ESP32-S3 LEDC PWM) |
| GPIO15/GPIO16 | I2C SDA/SCL | BME688 + TSL25911 + (camera SCCB) |
| GPIO17 | ONEWIRE | DS18B20 (nutrient solution temperature) |
| GPIO18 | FLOW_SENSOR | YF-S201 water flow sensor (interrupt) |
| GPIO19 | LED_RED_PWM | AL8860 #1 PWM dimming (deep red channel) |
| GPIO20 | LED_BLUE_PWM | AL8860 #2 PWM dimming (royal blue channel) |
| GPIO21 | LED_WHITE_PWM | AL8860 #3 PWM dimming (warm white channel) |
| GPIO26 | LED_FARRED_PWM | AL8860 #4 PWM dimming (far red channel) |
| GPIO27 | PUMP_MOSFET | IRLZ44N water pump drive (PWM for flow control) |
| GPIO33 | NUTRIENT_A_STEP | ULN2003 #1 stepper step (peristaltic pump A) |
| GPIO34 | NUTRIENT_A_DIR | ULN2003 #1 stepper direction |
| GPIO35 | NUTRIENT_A_EN | ULN2003 #1 stepper enable |
| GPIO36 | NUTRIENT_B_STEP | ULN2003 #2 stepper step (peristaltic pump B) |
| GPIO37 | NUTRIENT_B_DIR | ULN2003 #2 stepper direction |
| GPIO38 | NUTRIENT_B_EN | ULN2003 #2 stepper enable |
| GPIO39 | PH_STEP | DRV8833 pH doser step |
| GPIO40 | PH_DIR | DRV8833 pH doser direction |
| GPIO41 | PH_EN | DRV8833 pH doser enable |
| GPIO42 | FAN_PWM | Ventilation fan PWM speed control |
| GPIO43 | HEATER_SSR | SSR-25DA solid state relay (heat mat) |
| GPIO44 | HUMIDIFIER | Ultrasonic mist maker MOSFET |
| GPIO45 | RELAY_1 | Main water pump relay |
| GPIO46 | RELAY_2 | Exhaust fan relay |
| GPIO47 | RELAY_3 | Circulation fan relay |
| GPIO48 | RELAY_4 | Backup heater relay |
| GPIO5/6/7 | SPI2 SCK/MISO/MOSI | SX1261 Sub-GHz radio |
| GPIO8 | SX1261_NSS | Radio SPI chip select |
| GPIO9 | SX1261_BUSY | Radio busy signal |
| GPIO10 | SX1261_IRQ | Radio interrupt |
| GPIO11 | SX1261_NRST | Radio reset |
| GPIO12 | LED_R | Status LED red |
| GPIO13 | LED_G | Status LED green |
| GPIO14 | LED_B | Status LED blue |
| GPIO15 | BTN | Setup/pairing button |

### Plant Sensor Node (STM32WL55CC)

| Pin | Function | Connected To |
|-----|----------|--------------|
| PA0 | ADC_IN5 | Capacitive soil moisture sensor (analog output) |
| PA1 | ADC_IN6 | Soil EC excitation (AC square wave output, GPIO toggle) |
| PA2 | ADC_IN7 | Soil EC measurement (differential return) |
| PA3 | ADC_IN8 | Soil EC reference (differential reference) |
| PA4/PA5 | I2C1 SDA/SCL | TSL25911 (PAR light sensor) |
| PA6/PA7 | I2C2 SDA/SCL | (expansion: future I2C sensors) |
| PB6/PB7 | I2C3 SDA/SCL | (expansion or factory test) |
| PB10 | ONEWIRE_TX | DS18B20 soil temperature (OneWire bus) |
| PB11 | LEAF_WET_ADC | Capacitive leaf wetness sensor (analog) |
| PB12 | LED_DATA | WS2812B mini RGB LED data |
| PB13 | BTN | Pairing / manual read button (active low) |
| PB14 | SUBGHZ_RF_SET | Internal Sub-GHz RF control (STM32WL integrated) |
| PC0 | VBAT_SENSE | Battery voltage ADC (through resistor divider) |
| PA8 | LED_R | Status LED red (basic indicator, backup) |
| PA9 | LED_G | Status LED green |
| PA10 | LED_B | Status LED blue |
| PA13/PA14 | SWDIO/SWCLK | Debug/programming port |

### Weather Station Node (RP2040 + SX1262)

| Pin | Function | Connected To |
|-----|----------|--------------|
| GPIO0/GPIO1 | I2C0 SDA/SCL | SHT45 (temp/humidity) + BMP390 (pressure) |
| GPIO2/GPIO3 | I2C1 SDA/SCL | VEML6075 (UV) + TSL25911 (light) |
| GPIO4 | SPI0 SCK | SX1262 Sub-GHz radio |
| GPIO5 | SPI0 MISO | SX1262 SPI |
| GPIO6 | SPI0 MOSI | SX1262 SPI |
| GPIO7 | SX1262_NSS | Radio SPI chip select |
| GPIO8 | SX1262_BUSY | Radio busy signal |
| GPIO9 | SX1262_IRQ | Radio interrupt |
| GPIO10 | SX1262_NRST | Radio reset |
| GPIO11 | ANEMOMETER | Wind speed reed switch (interrupt, counter) |
| GPIO12 | WIND_VANE | Wind direction potentiometer (ADC) |
| GPIO13 | RAIN_GAUGE | Tipping bucket reed switch (interrupt, counter) |
| GPIO14 | SOLAR_VOLT | Solar panel voltage ADC (MPPT monitoring) |
| GPIO15 | BAT_VOLT | Lipo battery voltage ADC (SOC estimation) |
| GPIO16 | CHG_STATUS | MCP73871 charge status |
| GPIO17 | BQ25570_EN | Energy harvester enable |
| GPIO18 | LED_R | Status LED red |
| GPIO19 | LED_G | Status LED green |
| GPIO20 | LED_B | Status LED blue |
| GPIO21 | BTN | Setup/pairing button |
| GPIO22/23 | SWDIO/SWCLK | Debug (RP2040 SWD) |
| GPIO24 | UART0 TX | Debug console |
| GPIO25 | UART0 RX | Debug console |

---

## Power Architecture

### Hub Node
```
USB-C 5V ──► MCP73831 ──► Lipo 3000mAh ──► AP2112-3.3V ──► nRF5340 + ESP32-C6
                                     ──► AP6212-1.8V ──► SX1262
                                     ──► 5V direct   ──► TFT backlight + audio amp
                                     ──► AP2112-1.8V ──► Flash
Power budget:
  nRF5340 active:      ~30mA
  ESP32-C6 WiFi TX:    ~80mA (burst)
  SX1262 TX:           ~45mA (burst, 14dBm)
  TFT backlight:       ~50mA
  Audio amp (speech):  ~200mA (burst)
  Idle (display dim):  ~15mA
  Lipo backup:         ~8 hours (no WiFi, mesh + alerts only)
```

### Grow Pod Controller
```
24V DC barrel ──► LM2596-5V ──► ESP32-S3 + sensors + MOSFET logic
              ──► AL8860 #1-4 ──► LED strips (direct 24V drive, constant current)
              ──► 12V buck ──► Water pump + fans + peristaltic motors
USB-C 5V ──► Backup power (keeps MCU + sensors alive if 24V fails)
Power budget (full operation):
  ESP32-S3 + camera:    ~200mA @ 5V
  LED strips (4ch):      ~2A @ 24V (48W total, max)
  Water pump:            ~500mA @ 12V
  Peristaltic pumps:     ~200mA @ 12V (each, intermittent)
  Fans:                  ~300mA @ 12V
  Heater:                ~625mA @ 24V (15W)
  Total worst case:      ~3.5A @ 24V = 84W
```

### Plant Sensor Node
```
3× AA alkaline (4.5V) ──► HT7333-3.3V ──► STM32WL55CC + sensors
                       ──► 4.5V direct ──► Soil moisture + EC drive (VCC for sensors)
Solar variant:
2W 6V panel ──► MCP73871 ──► Lipo 600mAh ──► HT7333-3.3V ──► MCU + sensors
Power budget:
  STM32WL active:       ~8mA (measurement burst, 100ms every 5 min)
  Sub-GHz TX burst:     ~30mA (100ms every 5 min)
  Sensors:              ~5mA (during measurement)
  Sleep:                ~3µA (RTC only, between readings)
  Average:              ~300µA → 6+ months on 3× AA alkaline (2500mAh)
```

### Weather Station Node
```
5W 6V solar panel ──► BQ25570 (MPPT) ──► Lipo 2000mAh ──► AP2112-3.3V ──► RP2040 + sensors
                                       ──► 3.6V direct ──► SX1262
Power budget:
  RP2040 active:        ~15mA
  SX1262 TX:            ~45mA (burst, once per minute)
  Sensors:              ~5mA (continuous, SHT45 + BMP390 + VEML6075 + TSL25911)
  Sleep (night):        ~20µA
  Average (daylight):   ~25mA, fully sustained by 5W panel (5W/3.3V = 1.5A charge, far exceeds draw)
  Battery reserve:      2000mAh / 25mA = 80 hours without any sun
  Realistic:            72+ hours continuous overcast operation
```

---

## Software Stack

### Cloud Backend (FastAPI + MQTT)

```
software/dashboard/
├── app/
│   ├── main.py              # FastAPI app, MQTT subscriber, REST endpoints
│   ├── models.py            # SQLAlchemy models: Plant, Sensor, Reading, Alert, Harvest
│   ├── mqtt_handler.py      # Async MQTT client, message routing
│   ├── ml_inference.py      # Cloud ML inference (yield, nutrient, planting)
│   ├── image_processor.py   # Receive plant images, run disease detection
│   ├── weather_service.py   # External weather API integration
│   ├── irrigation_engine.py # RL-based irrigation scheduling
│   ├── nutrient_engine.py   # Nutrient dosing recommendation engine
│   └── requirements.txt     # FastAPI, paho-mqtt, sqlalchemy, torch, Pillow
├── docker-compose.yml       # API + MQTT broker + PostgreSQL + MinIO (image storage)
└── Dockerfile
```

**API Endpoints:**
- `GET /api/plants` — List all plants with current health status
- `GET /api/plants/{id}` — Detailed plant info with history
- `GET /api/plants/{id}/readings` — Time-series sensor data for a plant
- `POST /api/plants/{id}/water` — Manual irrigation command
- `POST /api/plants/{id}/nutrient` — Manual nutrient dosing
- `GET /api/garden/summary` — Garden-wide health dashboard
- `GET /api/weather` — Current outdoor conditions from weather station
- `GET /api/weather/forecast` — External forecast + local correction
- `POST /api/alerts/config` — Configure alert thresholds per plant
- `GET /api/harvest/predictions` — Upcoming harvest dates and yields
- `GET /api/planting/advice` — What to plant now for your location
- `POST /api/images/upload` — Upload plant photo for cloud disease analysis
- `WebSocket /ws/live` — Real-time sensor data stream

### Mobile App (React Native)

Key screens:
1. **Garden Dashboard** — Bird's-eye view of all plants with health color coding, quick stats
2. **Plant Detail** — Individual plant card: current readings, health trend graph, disease alerts, harvest countdown
3. **Grow Pod Control** — Light spectrum sliders (red/blue/white/far-red), fan speed, temperature setpoint
4. **Weather** — Current conditions, rain forecast, "Watering skipped — rain expected" notification
5. **Harvest Calendar** — What's ready now, what's coming, what to plant next
6. **Planting Advisor** — AI suggestions: "Plant lettuce now — 87% success rate for your area"
7. **Recipe Suggestions** — "Your basil is harvest-ready — try these 5 recipes"
8. **Alerts** — Push notifications for: disease detected, soil dry, pump failure, harvest ready, frost warning
9. **Settings** — Hub pairing, sensor calibration, alert thresholds, plant profiles

---

## Bill of Materials

### Hub Node BOM

| Qty | Part | Description | Unit Cost | Total |
|-----|------|-------------|-----------|-------|
| 1 | nRF5340 | MCU, dual Cortex-M33, BLE 5.0 | $6.50 | $6.50 |
| 1 | ESP32-C6-MINI-1 | WiFi 6 + BLE module | $3.20 | $3.20 |
| 1 | SX1262IMLTRT | Sub-GHz LoRa transceiver, 868MHz | $4.10 | $4.10 |
| 1 | ILI9341 3.2" TFT | 320×240 IPS display with touch | $5.50 | $5.50 |
| 1 | MAX98357A | I2S class-D amplifier | $1.20 | $1.20 |
| 1 | SPH0645LM4H | I2S MEMS microphone | $2.80 | $2.80 |
| 1 | BME688 | Environmental sensor (T/RH/P/VOC) | $3.50 | $3.50 |
| 1 | TSL25911FN | Light-to-digital sensor | $2.40 | $2.40 |
| 1 | W25Q128JVEIQ | 16MB NOR flash | $0.90 | $0.90 |
| 1 | MicroSD slot | SD card connector | $0.50 | $0.50 |
| 1 | MCP73831 | LiPo charger IC | $0.60 | $0.60 |
| 1 | AP2112-3.3 | 600mA LDO | $0.40 | $0.40 |
| 1 | AP6212-1.8 | 300mA LDO | $0.35 | $0.35 |
| 1 | 3W speaker | 8Ω, 40mm | $1.50 | $1.50 |
| 1 | Piezo buzzer | 3V, 85dB | $0.40 | $0.40 |
| 1 | USB-C receptacle | 16-pin, 3A | $0.50 | $0.50 |
| 1 | Lipo 3000mAh | 3.7V, 103040 | $5.00 | $5.00 |
| 5 | RGB LED 3528 | Common anode | $0.10 | $0.50 |
| 1 | PCB | 4-layer, 80×60mm | $2.50 | $2.50 |
| — | Passives | Resistors, caps, inductors | — | $2.00 |
| — | **Total** | | | **$42.85** |

### Grow Pod Controller BOM

| Qty | Part | Description | Unit Cost | Total |
|-----|------|-------------|-----------|-------|
| 1 | ESP32-S3-WROOM-1-N8R8 | Dual Xtensa, 8MB PSRAM, 8MB flash | $4.50 | $4.50 |
| 1 | OV2640 | 2MP camera module with IR-cut filter | $2.50 | $2.50 |
| 4 | AL8860MP-13 | Constant current buck LED driver, 1A | $1.10 | $4.40 |
| 1 | SX1261IMLTRT | Sub-GHz LoRa transceiver | $3.80 | $3.80 |
| 2 | 28BYJ-48 | 5V stepper motor (peristaltic pump drive) | $1.50 | $3.00 |
| 2 | ULN2003A | Darlington driver array (stepper) | $0.40 | $0.80 |
| 1 | 0622A002BLO | Micro peristaltic pump (pH dosing) | $3.50 | $3.50 |
| 1 | DRV8833 | Dual H-bridge (pH doser motor) | $1.20 | $1.20 |
| 1 | IRLZ44N | Logic-level MOSFET (water pump) | $0.60 | $0.60 |
| 1 | YF-S201 | Water flow sensor, 1-30 L/min | $2.00 | $2.00 |
| 1 | BME688 | Environmental sensor | $3.50 | $3.50 |
| 1 | TSL25911FN | PAR light sensor | $2.40 | $2.40 |
| 1 | DS18B20 | Waterproof temperature probe, 1m cable | $2.50 | $2.50 |
| 1 | SSR-25DA | Solid state relay 25A (heater) | $3.00 | $3.00 |
| 1 | Ultrasonic mist maker | 24V, 5W humidifier disc | $3.00 | $3.00 |
| 1 | 12V DC fan | 80mm, PWM speed control | $2.50 | $2.50 |
| 4 | OMRON G5LE-14 | 5V relay, 10A | $1.50 | $6.00 |
| 1 | LM2596-5.0 | Step-down converter 24V→5V, 3A | $1.20 | $1.20 |
| 1 | LM2596-12.0 | Step-down converter 24V→12V, 3A | $1.20 | $1.20 |
| 1 | USB-C receptacle | 16-pin, 3A | $0.50 | $0.50 |
| 1 | DC barrel jack | 5.5×2.1mm, 24V input | $0.30 | $0.30 |
| 1 | PCB | 4-layer, 120×80mm | $4.00 | $4.00 |
| 1 | Enclosure | IP54, DIN rail / shelf mount | $3.50 | $3.50 |
| — | LED strips | Full spectrum 4-channel (per pod, varies) | $15.00 | $15.00 |
| — | Passives | Resistors, caps, inductors, connectors | — | $3.00 |
| — | **Total** | | | **$74.40** |

### Plant Sensor Node BOM

| Qty | Part | Description | Unit Cost | Total |
|-----|------|-------------|-----------|-------|
| 1 | STM32WL55CC | MCU + Sub-GHz radio, Cortex-M4+M0+ | $4.80 | $4.80 |
| 1 | Capacitive soil moisture v1.2 | Analog, corrosion-resistant | $2.50 | $2.50 |
| 1 | DS18B20 | Waterproof temp probe, 30cm cable | $2.00 | $2.00 |
| 1 | TSL25911FN | Light-to-digital sensor | $2.40 | $2.40 |
| 1 | Custom leaf wetness PCB | Interdigitated copper trace, gold-plated | $1.50 | $1.50 |
| 1 | HT7333-3.3 | 250mA LDO, ultra-low Iq (4µA) | $0.40 | $0.40 |
| 1 | WS2812B-mini | 3.5×3.5mm RGB LED | $0.25 | $0.25 |
| 1 | 12mm tactile button | Waterproof boot | $0.30 | $0.30 |
| 1 | 868MHz whip antenna | 50mm, SMA connector | $0.80 | $0.80 |
| 1 | Battery holder | 3× AA, with door | $0.60 | $0.60 |
| 1 | PCB | 2-layer, 35×120mm (probe form factor) | $1.50 | $1.50 |
| 1 | Enclosure | IP68, ASA, 35×120×15mm | $2.00 | $2.00 |
| — | Passives | Resistors, caps, ESD protection | — | $0.80 |
| — | **Total** | | | **$19.85** |

### Weather Station Node BOM

| Qty | Part | Description | Unit Cost | Total |
|-----|------|-------------|-----------|-------|
| 1 | RP2040 | Dual Cortex-M0+, 133MHz | $1.20 | $1.20 |
| 1 | W25Q16JVSIQ | 2MB flash (RP2040 program storage) | $0.40 | $0.40 |
| 1 | SX1262IMLTRT | Sub-GHz LoRa transceiver, 868MHz | $4.10 | $4.10 |
| 1 | SHT45-AD1B | Temp/humidity, ±0.2°C, ±1.8%RH | $3.50 | $3.50 |
| 1 | BMP390 | Barometric pressure, ±0.5hPa | $2.80 | $2.80 |
| 1 | VEML6075 | UVA/UVB sensor | $2.20 | $2.20 |
| 1 | TSL25911FN | Ambient light sensor | $2.40 | $2.40 |
| 1 | Anemometer | Cup type, reed switch output | $8.00 | $8.00 |
| 1 | Wind vane | Potentiometer, 8-point | $6.00 | $6.00 |
| 1 | Rain gauge | Tipping bucket, 0.2794mm/tip | $12.00 | $12.00 |
| 1 | 5W 6V solar panel | Polycrystalline, 180×180mm | $5.00 | $5.00 |
| 1 | BQ25570RGRR | Energy harvester + MPPT + buck | $3.50 | $3.50 |
| 1 | MCP73871 | LiPo charge management | $1.50 | $1.50 |
| 1 | Lipo 2000mAh | 3.7V, 103040 | $4.00 | $4.00 |
| 1 | AP2112-3.3 | 600mA LDO | $0.40 | $0.40 |
| 1 | 868MHz chip antenna | With matching network | $0.60 | $0.60 |
| 1 | PCB | 4-layer, 100×70mm | $3.00 | $3.00 |
| 1 | Enclosure | IP65 Stevenson screen, ASA, UV-stable | $8.00 | $8.00 |
| 1 | Pole mount kit | U-bolt + bracket | $2.00 | $2.00 |
| — | Passives | Resistors, caps, connectors | — | $1.50 |
| — | **Total** | | | **$74.10** |

---

## Example System Configuration

A typical urban apartment balcony garden:

```
Balcony (3m × 1.2m):
  ├── Plant Sensor #1: Cherry tomatoes (15L pot)
  ├── Plant Sensor #2: Basil (window box)
  ├── Plant Sensor #3: Lettuce (rectangular planter)
  ├── Plant Sensor #4: Peppers (12L pot)
  ├── Plant Sensor #5: Mint (hanging basket)
  └── Weather Station: Mounted on railing

Living room (near balcony door):
  └── Hub Node: On shelf near balcony door

Spare closet (80×60×180cm grow tent):
  ├── Grow Pod Controller: Inside tent
  ├── Plant Sensor #6: Microgreens tray 1
  ├── Plant Sensor #7: Microgreens tray 2
  └── Plant Sensor #8: Herb propagation

Total nodes: 1 hub + 1 grow pod + 8 plant sensors + 1 weather station = 11 nodes
Total cost: ~$490 (nodes only, excluding pots, soil, seeds, and LED strips)

System manages:
  - Automatic watering of all 8 plants on optimal schedule
  - Nutrient dosing for grow tent herbs (hydroponic)
  - LED spectrum control for 4 growth stages
  - Disease monitoring via camera (grow tent) + leaf wetness (all)
  - Rain-based irrigation skip for balcony plants
  - Frost alerts for balcony (move sensitive plants inside)
  - Harvest prediction: "Basil ready in 5 days, tomatoes in 47 days"
```

---

## Quick Start

### Hardware Assembly
1. Build hub node PCB (see schematic/hub-node.sch)
2. Build plant sensor nodes (see schematic/plant-sensor.sch) — one per planter
3. Build grow pod controller (see schematic/grow-pod.sch) if using indoor grow space
4. Build weather station (see schematic/weather-station.sch) if growing outdoors
5. Flash firmware to each node (see scripts/deploy.sh)
6. Insert plant sensors into soil, connect grow pod to pumps/lights
7. Power on hub — it automatically discovers and pairs mesh nodes

### Software Setup
```bash
# Start cloud backend
cd software/dashboard
docker-compose up -d

# Train ML models (or use pre-trained)
cd software/ml-pipeline
python train_plant_disease.py
python train_yield_predict.py

# Build and run mobile app
cd software/mobile-app
npm install && npx react-native run-android
```

### First Plant
1. Open app → Add Plant → select type (tomato, basil, lettuce, etc.)
2. Assign a plant sensor → insert sensor into soil
3. App walks you through: pot size, soil type, location (indoor/outdoor/balcony)
4. System auto-configures irrigation schedule, light settings, nutrient profile
5. Watch your plant data appear on the dashboard within minutes

---

## License

MIT — plant it, grow it, sell it, improve it.

---

*Invented and maintained by [jayis1](https://github.com/jayis1). Part of the [Devices](https://github.com/jayis1/Devices) project.*