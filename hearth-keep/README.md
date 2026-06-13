# HearthKeep

**Ambient elder safety and wellness monitoring system — no cameras, full privacy.** Keeps your loved ones safe in their own home with invisible, continuous monitoring that respects dignity.

---

## What It Does

HearthKeep is a 4-node system that turns any home into a safe environment for aging adults — without cameras, without wearables-that-need-charging, without stigma:

1. **Detects** falls instantly using mmWave radar in every room — even in the bathroom where falls are most dangerous
2. **Monitors** sleep quality, heart rate, and breathing through an under-mattress pressure mat — zero wearables needed
3. **Tracks** daily activity patterns and alerts when routines change (sleeping more, moving less, unusual patterns)
4. **Provides** a panic pendant that works everywhere in the home — press for instant help
5. **Learns** what's normal for each person and only alerts when something's truly wrong
6. **Reports** wellness trends to family caregivers — sleep quality, activity levels, room usage patterns
7. **Respects** privacy absolutely — no cameras, no microphones recording conversations, no cloud audio storage

All room monitors and the bed mat communicate over a dedicated Sub-GHz mesh network (no WiFi dependency for life-critical fall detection). The hub bridges to WiFi/cloud for dashboard and mobile app. The wearable tag uses BLE to the hub.

### The Problem It Solves

- Falls are the #1 cause of injury death in adults 65+ — 36,000 deaths/year in the US alone
- 3 million older adults are treated in ERs for falls annually
- 60% of falls happen in the home; 80% go unwitnessed
- Camera-based monitoring violates privacy and dignity — most elders refuse it
- Wearable panic buttons have <40% adherence — people forget or refuse to wear them
- Social isolation and undetected health declines are epidemic among the elderly
- Family caregivers worry constantly but can't be there 24/7

HearthKeep detects falls **without anyone wearing anything**. The mmWave radar "sees" body position and movement without capturing images. The bed mat knows vitals without any strap or device. The panic tag is optional — the system works without it. Install it once, and it watches over your loved ones invisibly.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        HEARTHKEEP SYSTEM                                 │
│                                                                          │
│  ┌──────────────────┐   Sub-GHz    ┌──────────────────┐                │
│  │  ROOM MONITOR #1 │◄───────────►│                  │                │
│  │  (Living Room)   │   868MHz    │                  │                │
│  │  mmWave radar    │   mesh     │                  │                │
│  │  BME688 env      │            │                  │                │
│  │  Light sensor    │            │                  │                │
│  └──────────────────┘            │                  │                │
│                                  │                  │                │
│  ┌──────────────────┐            │   HUB NODE       │                │
│  │  ROOM MONITOR #2 │◄──────────►│  (nRF5340 +      │──── WiFi6 ────► Cloud
│  │  (Bathroom)      │   Sub-GHz  │   ESP32-C6)      │                Dashboard
│  │  mmWave radar    │   mesh     │                  │                + ML Pipeline
│  │  BME688 env      │            │                  │                + Alerts
│  │  IP65 rated      │            │                  │
│  └──────────────────┘            │                  │─── BLE ──────► Wearable Tag
│                                  │                  │                (Panic Pendant)
│  ┌──────────────────┐            │                  │
│  │  ROOM MONITOR #3 │◄──────────►│                  │
│  │  (Kitchen)       │   Sub-GHz  │                  │
│  │  mmWave radar    │   mesh     │                  │
│  │  BME688 env      │            │                  │
│  └──────────────────┘            │                  │
│                                  │                  │
│  ┌──────────────────┐            │                  │
│  │  BED MAT NODE   │◄──────────►│                  │
│  │  (Under mattress│   Sub-GHz  └──────┬───────────┘
│  │   pressure strip│   mesh            │
│  │   8× FSR sensors│                  │ Sub-GHz mesh
│  │   Heart+breath   │                  │ (up to 16 room monitors
│  └──────────────────┘                  │  per hub)
│                                        │
│  ┌──────────────────┐                 │
│  │  WEARABLE TAG    │◄──── BLE ───────┘
│  │  (Keychain fob)  │
│  │  Panic button    │
│  │  Accelerometer   │
│  │  CR2032 battery  │
│  └──────────────────┘
│
│  ┌──────────────────────────────────────────────────────────────────┐
│  │                    CLOUD / EDGE SOFTWARE                       │
│  │  ┌──────────┐  ┌──────────────┐  ┌───────────────────────┐    │
│  │  │Dashboard │  │ ML Pipeline  │  │ Mobile App            │    │
│  │  │ (React)  │  │ Fall detect  │  │ (React Native)        │    │
│  │  │ Realtime │  │ Activity     │  │ Push alerts           │    │
│  │  │ History  │  │ Anomaly      │  │ Wellness trends       │    │
│  │  │ Config   │  │ Health trend │  │ Two-way voice (hub)   │    │
│  │  └──────────┘  └──────────────┘  └───────────────────────┘    │
│  └──────────────────────────────────────────────────────────────────┘
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per home)

The brain. Bridges Sub-GHz mesh to WiFi/BLE/cloud. Runs local ML models. Provides voice interface and emergency button.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF5340 (dual Cortex-M33) | Application core runs mesh + sensors; network core runs BLE 5.0 |
| WiFi Bridge | ESP32-C6-MINI-1 | WiFi 6 + BLE bridge to cloud |
| Radio | SX1262 (868MHz) | Sub-GHz LoRa mesh coordinator to all nodes |
| Display | 3.2" IPS TFT (ILI9341) | Local status: room occupancy, alerts, last activity |
| Audio Out | MAX98357A (I2S amp) + 3W speaker | Two-way voice, alarm tones, spoken status |
| Audio In | SPH0645LM4H (I2S MEMS mic) | Voice commands, two-way voice with caregiver |
| Storage | 16MB W25Q128 Flash + SD card | Data logging, OTA updates, audio buffer |
| Emergency Button | Large 30mm tactile (red) | Physical panic/emergency trigger |
| LEDs | RGB status LED + 4× zone LEDs | Visual status indicators |
| Power | 5V USB-C + Lipo 3000mAh | Stays running during power outage (8+ hours) |
| Connectors | 2× I2C, 1× UART, 1× SPI, 8× GPIO | Expansion |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler, assigns slots to all nodes)
- Aggregates data from all room monitors and bed mat
- Runs TFLite Micro fall detection classifier (radar point cloud → fall/no-fall)
- Runs activity recognition model (motion patterns → walking/sitting/lying/cooking/etc.)
- WiFi uplink to MQTT broker (QoS 1, TLS)
- BLE GATT server for wearable tags and mobile app
- TFT dashboard rendering (room status, last activity time, alert history)
- Two-way voice: speaker + mic for caregiver communication
- Emergency response: alarm + voice prompt + cloud alert on fall/panic
- OTA update distribution to all nodes
- Local alarm triggers even when WiFi is down

### 2. Room Monitor Node (1-16 per home)

Wall-mounted mmWave radar presence and fall detector. The core of the system. No cameras — radar "sees" body position without capturing images.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52833 | BLE 5.0, 64MHz Cortex-M4, 512KB Flash, 128KB RAM |
| mmWave Radar | Infineon BGT60TR13C | 60GHz FMCW, 1Tx/3Rx, presence + fall detection + breathing |
| Environment | Bosch BME688 | Temperature, humidity, pressure, VOC/IAQ |
| Light | TSL25911 | Ambient light (lux) — detects day/night patterns |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Antenna | 868MHz chip antenna + 60GHz patch | Integrated antennas |
| Power | 5V USB-C or 3× AA (4.5V) backup | Mains powered with battery backup |
| Enclosure | IP20 (indoor) or IP65 (bathroom) | 60×60×18mm wall-mount |

**Room monitor firmware:**
- BGT60TR13C radar runs continuous presence detection (100ms cycle)
- On motion: increase radar resolution to point cloud mode (10Hz)
- Point cloud processed locally: classify as standing/sitting/lying/falling
- If fall detected: immediate alert to hub via mesh (<500ms)
- Environmental readings every 30 seconds (temp, humidity, IAQ, light)
- On-board TFLite Micro fall classifier (INT8, 80KB)
- Mesh TDMA time-slot transmission to hub
- Ultra-low-power idle between readings (~200µA)
- Self-calibration on power-up (learns room dimensions)

### 3. Bed Mat Node (1 per bed)

Thin strip that goes under the mattress. Detects heart rate, breathing, movement, in/out of bed, sleep quality — all through the mattress.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32L476RG | Ultra-low-power ARM Cortex-M4, 12-bit ADC |
| Pressure Sensors | 8× FSR-402 (Interlink) | Force-sensitive resistors, ballistocardiographic heart rate |
| Reference ADC | ADS1256 (24-bit, 8-ch) | High-resolution ADC for pressure waveform capture |
| Amplifier | 8× HX711 (24-bit load cell amp) | One per FSR, digitizes pressure waveform |
| MUX | CD74HC4067 (16:1) | Multiplex HX711 outputs to MCU |
| Radio | SX1261 (868MHz) | Sub-GHz mesh client |
| Temperature | DS18B20 (waterproof) | Mattress temperature |
| Power | Lipo 2000mAh + USB-C charging | 2-week battery life between charges |
| Enclosure | Flexible PCB strip, 800×60×5mm | Slips under mattress |

**Bed mat firmware:**
- Reads all 8 FSR sensors at 250Hz (for heart rate ballistocardiography)
- 24-bit ADC captures micro-movements from heartbeat through mattress
- DSP pipeline: bandpass filter (0.8-2.5 Hz for heart, 0.15-0.5 Hz for breathing)
- Extracts: heart rate, breathing rate, movement index, in-bed status
- Reports vitals to hub every 30 seconds while in bed
- Reports sleep phase estimates (light/deep/REM from movement patterns)
- Mesh TDMA transmission to hub
- Ultra-low-power mode when bed unoccupied (~5µA)
- Auto-calibration: learns mattress firmness and patient weight on first use

### 4. Wearable Tag (1-4 per person)

Optional keychain/fob with panic button. Works everywhere in the home via BLE to hub. Accelerometer provides secondary fall detection.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52810 | Ultra-low-power BLE 5.0, Cortex-M4 |
| Accelerometer | LIS2DH12 | 3-axis, ultra-low-power fall detection |
| Button | 12mm tactile | Panic/emergency trigger |
| Buzzer | 12mm piezo SMD | Local alarm beep on panic |
| LED | RGB LED (WS2812B mini) | Status indicator |
| Power | CR2032 coin cell | 6+ month battery life |
| Antenna | PCB trace antenna | BLE range ~10m indoor |
| Enclosure | Keychain fob, 42×28×10mm | ABS, IP54 |

**Wearable tag firmware:**
- BLE 5.0 connectionless mode: broadcasts presence every 2 seconds
- Panic button: immediate BLE alert to hub on press
- Accelerometer: always-on 25Hz sampling, fall detection interrupt
- If fall detected by accelerometer: BLE alert to hub
- Long-press (3s) for "I'm OK" cancel
- Battery voltage monitoring, reports low battery to hub
- Advertising packet includes: tag ID, battery %, panic status, fall flag
- Average current: 15µA (connectionless BLE advertising at 2s interval)

---

## Communication Protocol

### Sub-GHz Mesh (SX1262/61, 868MHz LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa SF7 (normal) / SF9 (long range alert) |
| Bandwidth | 125 kHz |
| TX Power | +14 dBm (EU) / +20 dBm (US) |
| Range | 30m indoor (normal) / 150m (long range) |
| Protocol | Custom TDMA (hub is coordinator) |
| Slot Duration | 50ms per node |
| Cycle Time | 1 second (16 data slots + 4 control) |

### TDMA Frame Structure

```
| SLOT 0 (HUB) | SLOT 1 (RM1) | SLOT 2 (RM2) | ... | SLOT 8 (RM8) | SLOT 9 (BM) | S10-S15 (RM9-14) | SLOT 16 (FEED) | S17 (CTRL) |
|   50ms       |    50ms      |    50ms      |     |    50ms      |    50ms     |    50ms          |    50ms        |   50ms     |

Total frame: 900ms (18 slots × 50ms)
Slot 0: Hub broadcasts sync + commands + acknowledgment
Slots 1-8: Room monitors 1-8 uplink radar + environment data
Slot 9: Bed mat uplink vitals data
Slots 10-15: Room monitors 9-14 (if present)
Slot 16: Reserved / expansion
Slot 17: Control/ACK/retransmit/alert broadcast

ALERT OVERRIDE: Fall detection triggers immediate transmission
on slot 17 regardless of TDMA schedule (CSMA fallback).
```

### Mesh Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | SEQ(2) | PAYLOAD(0-48) | CRC16(2) ]

TYPE values:
  0x01 = RADAR_DATA     (presence count, fall score, movement level, position class)
  0x02 = ENV_DATA       (temp, humidity, pressure, IAQ, light)
  0x03 = BED_VITALS     (heart rate, breathing rate, movement index, in-bed, sleep phase)
  0x04 = COMMAND        (hub → node: configure, calibrate, OTA)
  0x05 = ACK            (acknowledgment)
  0x06 = OTA_BLOCK       (firmware update chunk)
  0x07 = FALL_ALERT      (CRITICAL — immediate transmission, bypasses TDMA)
  0x08 = PANIC_ALERT     (from wearable tag via hub relay)
  0x09 = HEARTBEAT       (periodic alive signal)
  0x0A = CALIBRATION     (radar calibration data, pressure calibration)
  0x0B = LOW_BATTERY    (battery warning)
```

### BLE Protocol (Wearable Tag ↔ Hub)

```
GATT Service: HearthKeep Tag (0xHEAR)
  Characteristic 0xHE01: Panic Status (notify, 1 byte: 0=idle, 1=panic, 2=cancel)
  Characteristic 0xHE02: Fall Status (notify, 1 byte: 0=normal, 1=fall_detected)
  Characteristic 0xHE03: Battery Level (notify, 1 byte: 0-100%)
  Characteristic 0xHE04: Tag Config (write, 4 bytes: sensitivity, LED mode, buzzer vol)

Advertising Packet (connectionless, every 2s):
  [ Flags(3) | Complete Local Name("HK-TAG-XXXX") | Manufacturer Data:
    [ CompanyID(0xHEAR) | TagID(2) | BatteryPct(1) | PanicStatus(1) | FallStatus(1) ] ]
```

---

## AI / ML Pipeline

### 1. Fall Detection (on Room Monitor, TFLite Micro)

- **Input**: Radar point cloud from BGT60TR13C (range-Doppler map, 16×32, 10 Hz during active)
- **Model**: MobileNetV2-0.1 backbone + 1D-CNN temporal head, INT8 quantized, 75KB
- **Output**: Fall probability (0-1), position class (standing/sitting/lying/falling)
- **Triggers**: Fall prob >0.85 → immediate alert to hub (bypasses TDMA)
- **False positive reduction**: 3-frame confirmation (must see fall posture for >300ms)
- **Confusion matrix target**: >97% sensitivity, >99% specificity on elderly fall dataset
- **Training data**: Augment with simulated falls, near-falls, and ADL (activities of daily living)

### 2. Activity Recognition (on Hub, TFLite Micro)

- **Input**: Aggregated motion patterns from all room monitors (occupancy changes over 60-second windows)
- **Model**: 1D-CNN + LSTM, INT8 quantized, 120KB
- **Output**: Activity class (walking, sitting, lying, cooking, bathroom use, exercising, unknown)
- **Purpose**: Build daily activity timeline, detect routine changes
- **Runs**: Every 5 minutes on aggregated data

### 3. Anomaly Detection (Cloud, PyTorch)

- **Input**: 24-hour activity timeline features (wake time, sleep duration, bathroom visits, kitchen activity, movement index)
- **Model**: Transformer-based sequence model, autoencoder architecture
- **Output**: Anomaly score per day, indicating deviation from learned normal pattern
- **Triggers**: Score >2σ → "Routine change detected" alert to caregivers
- **Personalization**: Trains on 14 days of data per person, continuously updates
- **Detects**: Sleeping more/less, reduced kitchen activity (may indicate depression/illness), increased bathroom visits (UTI risk), unusual wandering (confusion/delirium)

### 4. Health Trend (Cloud, PyTorch)

- **Input**: Longitudinal heart rate, breathing rate, sleep quality, movement indices
- **Model**: LSTM regression with attention, predicts 7-day trends
- **Output**: Trend graphs (sleep quality declining, heart rate increasing, etc.)
- **Triggers**: Statistically significant trends → "Health trend" alert
- **Privacy**: All trend data is aggregated; no raw waveforms stored in cloud

### 5. Room-Level Presence (on Hub, rule-based + ML refinement)

- **Input**: Radar presence signals from all room monitors
- **Output**: Room-level occupancy map (which rooms are occupied, how many people)
- **Purpose**: Track room transitions for activity monitoring; detect "no movement in expected room" alerts
- **Refinement**: ML learns individual movement patterns (Person A always goes to kitchen at 7 AM)

---

## Pin Assignments

### Hub Node (nRF5340 + ESP32-C6)

**nRF5340 (application core + mesh coordination):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.00/P0.01 | UART0 TX/RX | ESP32-C6 UART1 (inter-MCU link) |
| P0.02/P0.03 | I2C0 SDA/SCL | BME688 (on-board env sensor) |
| P0.04/P0.05 | I2C1 SDA/SCL | SX1262 Sub-GHz radio |
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
| P0.26 | EMERGENCY_BTN | Large red emergency button (active low) |
| P0.27 | LED_R | RGB status LED red |
| P0.28 | LED_G | RGB status LED green |
| P0.29 | LED_B | RGB status LED blue |
| P0.30/P0.31 | ZONE1/ZONE2 | Zone indicator LEDs |
| P1.00/P1.01 | ZONE3/ZONE4 | Zone indicator LEDs |

**ESP32-C6 (WiFi/BLE bridge):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO4/GPIO5 | UART1 TX/RX | nRF5340 UART0 |
| GPIO12/GPIO13 | USB D+/D- | USB-C port |
| GPIO6-11 | SPI | Flash (internal) |
| GPIO0/GPIO1 | I2C SDA/SCL | (expansion port) |

### Room Monitor Node (nRF52833)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02/P0.03 | I2C0 SDA/SCL | BGT60TR13C (mmWave radar) |
| P0.04/P0.05 | I2C1 SDA/SCL | BME688 (environment) |
| P0.06 | I2C0 SCL_ALT | TSL25911 (light sensor, same bus as radar) |
| P0.10/P0.11 | SPI0 SCK/MOSI | SX1261 Sub-GHz radio |
| P0.12 | SPI0 MISO | SX1261 MISO |
| P0.13 | SPI0 CS | SX1261 NSS |
| P0.14 | SX1261_BUSY | Radio busy signal |
| P0.15 | SX1261_IRQ | Radio interrupt |
| P0.16 | SX1261_NRST | Radio reset |
| P0.17 | BGT_IRQ | Radar interrupt (presence/fall detected) |
| P0.18 | BGT_RST | Radar reset |
| P0.19 | LED_R | Status LED red |
| P0.20 | LED_G | Status LED green |
| P0.21 | LED_B | Status LED blue |
| P0.22 | BTN | Setup/pairing button |
| P0.26 | VBAT_SENSE | Battery/supply voltage ADC |
| P0.29/P0.30 | SWDIO/SWCLK | Debug port |

### Bed Mat Node (STM32L476RG)

| Pin | Function | Connected To |
|-----|----------|-------------|
| PA0-PA7 | ADC1 CH0-7 | HX711 DOUT×8 (pressure sensor amplifiers) |
| PA8 | TIM1_CH1 | HX711 SCK (shared clock, MUX-selected) |
| PA9/PA10 | UART1 TX/RX | Debug console |
| PB6/PB7 | I2C1 SDA/SCL | SX1261 Sub-GHz radio |
| PA4 | ADC1_CH4 | MUX output (CD74HC4067) |
| PB10/PB11 | UART3 TX/RX | (reserved for expansion) |
| PA8 | ONE_WIRE | DS18B20 temperature |
| PC0-PC3 | GPIO_OUT | CD74HC4067 select lines S0-S3 |
| PC4 | HX711_RATE | HX711 output data rate select |
| PC5 | CHG_STATUS | USB-C charge status |
| PB0 | VBAT_SENSE | Battery voltage ADC |
| PC6 | CHG_EN | Charge enable |
| PC7 | LED_STATUS | Status LED (bi-color) |
| PB3 | BTN | Setup/pairing button |

### Wearable Tag (nRF52810)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.00/P0.01 | I2C SDA/SCL | LIS2DH12 accelerometer |
| P0.02 | LIS2DH_INT1 | Accelerometer interrupt 1 (activity/inactivity) |
| P0.03 | LIS2DH_INT2 | Accelerometer interrupt 2 (click/double-tap) |
| P0.09 | SPI0 SCK | (reserved) |
| P0.11 | BTN_PANIC | 12mm panic button (active low, debounced) |
| P0.13 | PIEZO | Piezo buzzer PWM |
| P0.14 | LED_DATA | WS2812B mini RGB LED data |
| P0.17 | VBAT_SENSE | CR2032 voltage ADC (through voltage divider) |
| P0.25 | SWDIO | Debug/programming |
| P0.26 | SWCLK | Debug/programming |

---

## Power Architecture

### Hub Node
```
USB-C 5V ──► MCP73831 ──► Lipo 3000mAh ──► AP2112-3.3V ──► nRF5340 + ESP32-C6
                                       ──► AP6212-1.8V ──► SX1262
                                       ──► 5V direct ──► MAX98357A (speaker amp)
                                       ──► 5V direct ──► TFT backlight (via MOSFET)
```
- Average draw: 200mA (WiFi on + display) → ~15 hours on battery
- Battery backup: auto-fails to battery on USB loss, mesh keeps running
- Emergency mode: display off, WiFi off, mesh-only → 40+ hours on battery

### Room Monitor Node
```
USB-C 5V ──► AP2112-3.3V ──► nRF52833 + SX1261 + BGT60TR13C + BME688 + TSL25911
          ──► AP2112-3.3V ──► (shared rail, decoupled per-IC)
          
3× AA (4.5V) ──► AP2112-3.3V ──► (battery backup, ~48 hours)
```
- Average draw: 25mA (radar active) → ~60 hours on AA batteries
- Mains: USB-C 5V, battery backup auto-switches on power loss
- Radar sleep between scans: ~8mA average

### Bed Mat Node
```
Lipo 3.7V ──► MCP73831 ──► Lipo 2000mAh ──► AP2112-3.3V ──► STM32L476 + SX1261
                                                 ──► AP2112-3.3V ──► HX711×8 + DS18B20
USB-C 5V ──► MCP73831 (charging)
```
- Average draw: 15mA (ADC sampling at 250Hz + mesh TX) → ~5.5 days on battery
- Low-power mode (bed empty): 50µA → 4+ years theoretical
- Charges via USB-C in ~3 hours

### Wearable Tag
```
CR2032 3V ──► nRF52810 (direct, no regulator needed)
          ──► LIS2DH12 (direct)
          ──► Piezo buzzer (direct, low voltage)
          ──► WS2812B mini (direct, brief pulses only)
```
- Average draw: 15µA (BLE advertising at 2s interval + accel at 25Hz)
- CR2032 capacity: 225mAh → theoretical 15,000 hours (~625 days)
- Realistic: 6+ months with normal use including panic alarms

---

## Mechanical Design

### Hub Node
- Enclosure: 130×90×25mm ABS plastic (3D printed or injection)
- Wall-mountable (keyhole slots) or desktop stand
- 3.2" TFT visible through front window
- Large 30mm red emergency button on top
- Speaker grille on front
- Microphone port on front (3mm hole + acoustic mesh)
- USB-C port on bottom
- External SMA antenna connector for Sub-GHz
- 4 zone LEDs across top edge

### Room Monitor Node
- Enclosure: 60×60×18mm ABS plastic (indoor) / IP65 polycarbonate (bathroom)
- Wall-mounted at 1.5-2m height with 30° downward tilt
- mmWave radar antenna behind thin plastic radome (front face)
- BME688 vented through side slots (moisture-protected with PTFE membrane)
- TSL25911 light sensor behind clear window
- Status LED visible through diffuser
- USB-C port on bottom
- Magnetic mount: 3M adhesive plate + magnet
- Install angle: 30° downward for optimal floor coverage

### Bed Mat Node
- Flexible PCB strip: 800×60×5mm
- 8 FSR sensors distributed along length at 100mm intervals
- HX711 amplifiers on flexible PCB
- MCU and radio in rigid section at one end (50×60×12mm)
- Covered in thin (1mm) closed-cell foam for comfort
- Slips between mattress and box spring/slat
- USB-C charging port on rigid section (accessible from bed edge)
- Antenna: 868MHz wire antenna extends along strip edge

### Wearable Tag
- Enclosure: 42×28×10mm ABS, keychain form factor
- Large 12mm panic button on front (easy to press, requires deliberate action)
- RGB LED indicator visible through top
- Lanyard hole (neck strap or keychain)
- CR2032 battery door on back (tool-free replacement)
- IP54 (splash resistant)
- Available in 4 colors for multi-person households

---

## Full BOM

### Hub Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | nRF5340 | QFN-94 7×7 | 1 | $3.80 | $3.80 |
| 2 | ESP32-C6-MINI-1 | Module | 1 | $3.20 | $3.20 |
| 3 | SX1262 | QFN-24 | 1 | $4.50 | $4.50 |
| 4 | 3.2" IPS TFT (ILI9341) | Module | 1 | $8.50 | $8.50 |
| 5 | 16MB W25Q128 | SOIC-8 | 1 | $1.50 | $1.50 |
| 6 | SD card slot | Micro push-push | 1 | $0.50 | $0.50 |
| 7 | MAX98357A | QFN-16 | 1 | $1.20 | $1.20 |
| 8 | SPH0645LM4H | MEMS mic module | 1 | $2.80 | $2.80 |
| 9 | 3W speaker | 40mm round | 1 | $1.50 | $1.50 |
| 10 | MCP73831 | SOT-23-5 | 1 | $0.40 | $0.40 |
| 11 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 12 | AP6212-1.8 | SOT-23-5 | 1 | $0.35 | $0.35 |
| 13 | Lipo 3000mAh | Custom pouch | 1 | $5.50 | $5.50 |
| 14 | USB-C receptacle | 16-pin SMD | 1 | $0.35 | $0.35 |
| 15 | SMA connector | Edge-mount | 1 | $0.80 | $0.80 |
| 16 | Antenna 868MHz | Wire/PCB | 1 | $1.50 | $1.50 |
| 17 | Emergency button | 30mm tactile | 1 | $0.80 | $0.80 |
| 18 | RGB LED | WS2812B-2020 | 1 | $0.15 | $0.15 |
| 19 | Zone LEDs (4×) | 0805 LED | 4 | $0.05 | $0.20 |
| 20 | Passives (R/C/L/inductors) | 0402 | ~70 | $1.75 | $1.75 |
| 21 | PCB 4-layer | 130×90mm | 1 | $3.50 | $3.50 |
| | | | | **Subtotal** | **$42.00** |

### Room Monitor Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | nRF52833 | QFN-48 7×7 | 1 | $2.80 | $2.80 |
| 2 | BGT60TR13C | LGA-32 | 1 | $5.50 | $5.50 |
| 3 | BME688 | LGA-8 | 1 | $3.20 | $3.20 |
| 4 | TSL25911 | DFN-6 | 1 | $1.80 | $1.80 |
| 5 | SX1261 | QFN-24 | 1 | $3.80 | $3.80 |
| 6 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 7 | 3× AA battery holder | SMD clip | 1 | $0.50 | $0.50 |
| 8 | USB-C receptacle | 16-pin SMD | 1 | $0.35 | $0.35 |
| 9 | 60GHz patch antenna | Custom PCB | 1 | $0.50 | $0.50 |
| 10 | 868MHz chip antenna | Ceramic | 1 | $0.80 | $0.80 |
| 11 | RGB LED | 0505 SMD | 1 | $0.10 | $0.10 |
| 12 | Tactile button | 6mm SMD | 1 | $0.05 | $0.05 |
| 13 | Passives | 0402 | ~40 | $1.00 | $1.00 |
| 14 | PCB 4-layer | 55×55mm | 1 | $2.50 | $2.50 |
| 15 | Enclosure (ABS) | 60×60×18mm | 1 | $1.50 | $1.50 |
| 16 | Magnetic mount | 3M adhesive + magnet | 1 | $0.80 | $0.80 |
| | | | | **Subtotal** | **$25.50** |

### Bed Mat Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | STM32L476RG | LQFP-64 | 1 | $5.80 | $5.80 |
| 2 | SX1261 | QFN-24 | 1 | $3.80 | $3.80 |
| 3 | FSR-402 (Interlink) | Force sensor | 8 | $2.50 | $20.00 |
| 4 | HX711 | SOP-16 | 8 | $0.80 | $6.40 |
| 5 | CD74HC4067 | TSSOP-24 | 1 | $0.60 | $0.60 |
| 6 | DS18B20 | TO-92 | 1 | $1.20 | $1.20 |
| 7 | MCP73831 | SOT-23-5 | 1 | $0.40 | $0.40 |
| 8 | AP2112-3.3 | SOT-223 | 2 | $0.30 | $0.60 |
| 9 | Lipo 2000mAh | Custom flat | 1 | $4.00 | $4.00 |
| 10 | USB-C receptacle | 16-pin SMD | 1 | $0.35 | $0.35 |
| 11 | 868MHz wire antenna | Wire | 1 | $0.30 | $0.30 |
| 12 | RGB LED | 0505 SMD | 1 | $0.10 | $0.10 |
| 13 | Tactile button | 6mm SMD | 1 | $0.05 | $0.05 |
| 14 | Passives | 0402 | ~50 | $1.25 | $1.25 |
| 15 | Flexible PCB (800×60mm) | 2-layer FPC | 1 | $5.00 | $5.00 |
| 16 | Rigid PCB section (50×60mm) | 4-layer | 1 | $2.50 | $2.50 |
| 17 | Closed-cell foam cover | 800×60×1mm | 1 | $0.50 | $0.50 |
| | | | | **Subtotal** | **$53.65** |

### Wearable Tag

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | nRF52810 | QFN-32 5×5 | 1 | $1.80 | $1.80 |
| 2 | LIS2DH12 | LGA-12 | 1 | $0.90 | $0.90 |
| 3 | 12mm tactile button | Through-hole | 1 | $0.10 | $0.10 |
| 4 | Piezo buzzer | 12mm SMD | 1 | $0.30 | $0.30 |
| 5 | WS2812B-mini | LED | 1 | $0.15 | $0.15 |
| 6 | CR2032 holder | SMD | 1 | $0.20 | $0.20 |
| 7 | CR2032 battery | Coin cell | 1 | $0.30 | $0.30 |
| 8 | PCB trace antenna | On-board | 1 | $0.00 | $0.00 |
| 9 | Passives | 0402 | ~15 | $0.40 | $0.40 |
| 10 | PCB 2-layer | 38×26mm | 1 | $1.00 | $1.00 |
| 11 | Enclosure (ABS) | 42×28×10mm | 1 | $1.20 | $1.20 |
| 12 | Lanyard | Nylon | 1 | $0.15 | $0.15 |
| | | | | **Subtotal** | **$6.50** |

### System Total (1 hub + 4 room monitors + 1 bed mat + 2 wearable tags)

**Hardware BOM: ~$163.20**

Room monitors are the bulk of cost. A 4-room system costs under $165 in components — dramatically less than medical alert systems ($300-600) while providing far more capability.

---

## Software Stack

### Cloud Dashboard (React + FastAPI)

```
software/dashboard/
├── frontend/              # React + Vite + TailwindCSS
│   ├── src/
│   │   ├── components/    # Room cards, vital signs, alerts, timeline
│   │   ├── hooks/         # Real-time MQTT subscription
│   │   ├── pages/         # Dashboard, Activity, Vitals, Alerts, Settings
│   │   └── App.tsx
│   └── package.json
├── backend/               # FastAPI (Python)
│   ├── main.py            # REST + WebSocket server
│   ├── models.py          # SQLAlchemy home/sensor/alert models
│   ├── mqtt_bridge.py     # MQTT → DB + WebSocket relay
│   ├── activity_engine.py # Activity timeline builder
│   ├── alert_engine.py    # Alert routing and escalation
│   └── requirements.txt
└── docker-compose.yml     # Postgres + Mosquitto + API + Frontend
```

### ML Pipeline (Python)

```
software/ml-pipeline/
├── train_fall_detect.py   # Train radar fall detection model (MobileNetV2 + 1D-CNN)
├── train_activity.py      # Train activity recognition (1D-CNN + LSTM)
├── train_anomaly.py        # Train routine anomaly detector (Transformer autoencoder)
├── train_health_trend.py  # Train health trend predictor (LSTM + attention)
├── export_tflite.py       # Quantize and export to TFLite Micro
├── datasets.py            # Data loading and augmentation
├── models.py              # PyTorch model definitions
└── requirements.txt
```

### Mobile App (React Native)

```
software/mobile-app/
├── App.tsx                # Main app with navigation
├── screens/
│   ├── DashboardScreen    # Real-time status of all rooms
│   ├── AlertsScreen       # Alert history and management
│   ├── VitalsScreen       # Heart rate, breathing, sleep quality graphs
│   ├── ActivityScreen     # Daily activity timeline
│   ├── SettingsScreen     # System configuration, sensitivity, contacts
│   └── VoiceScreen        # Two-way voice to hub
├── components/
│   ├── RoomCard           # Room status card (occupied, last activity)
│   ├── VitalChart         # Interactive vital sign chart
│   ├── ActivityTimeline   # Visual daily activity timeline
│   └── AlertCard          # Alert notification card
└── package.json
```

---

## Alert Escalation Protocol

HearthKeep uses a multi-tier alert system that respects the user's dignity while ensuring safety:

### Tier 1: Local Verification (0-30 seconds)
- Fall detected by room monitor → hub plays gentle voice prompt: "It looks like you may have fallen. Are you okay?"
- If user presses wearable tag "I'm OK" or hub button within 30 seconds → cancel alert
- If radar detects user standing up → cancel alert
- No notification sent to caregivers

### Tier 2: Caregiver Alert (30-120 seconds)
- No "I'm OK" received → send push notification to caregiver's phone
- Notification includes: room, time, fall probability score, last known position
- Caregiver can: view live room status, initiate two-way voice, call 911, dismiss

### Tier 3: Emergency Escalation (2-5 minutes)
- No caregiver response → call caregiver's phone (auto-dial)
- Still no response → call second caregiver
- Still no response → call emergency services (configurable)

### Tier 4: Health Trend Alerts (daily/weekly)
- Routine change detected → daily summary notification
- Health trend declining → weekly report with recommendations
- Never false-positive emergency alerts for slow trends

---

## Privacy by Design

HearthKeep is built on the principle that **safety and privacy are not trade-offs**:

1. **No cameras anywhere** — mmWave radar detects presence and falls without capturing images
2. **No cloud audio storage** — microphone is only active during explicit voice sessions or alarm verification
3. **On-device ML** — fall detection and activity recognition run locally on the room monitor and hub
4. **Minimal data uplink** — only processed results (fall detected, heart rate, room occupied) are sent to cloud, never raw sensor data
5. **Encrypted transport** — all data TLS-encrypted in transit
6. **Encrypted storage** — all data AES-256 encrypted at rest
7. **User data ownership** — full data export and deletion available
8. **No third-party sharing** — data shared only with designated caregivers
9. **Local fallback** — all critical functions (fall detection, alarm) work without internet
10. **Open-source firmware** — full firmware source available for security audit

---

## Comparison with Alternatives

| Feature | HearthKeep | Medical Alert Pendant | Smart Camera | Life Alert |
|---------|-----------|----------------------|-------------|------------|
| Fall detection | ✅ Automatic (radar) | ❌ Must press button | ⚠️ Camera-based (privacy) | ✅ But only with pendant |
| Privacy | ✅ No cameras | ✅ Private | ❌ Cameras everywhere | ✅ Private |
| Wearable required | ⚠️ Optional | ❌ Must wear pendant | ✅ Not needed | ❌ Must wear |
| Heart rate monitoring | ✅ Through mattress | ❌ | ❌ | ❌ |
| Activity tracking | ✅ Room-level | ❌ | ⚠️ Camera-based | ❌ |
| Sleep monitoring | ✅ Heart + breathing | ❌ | ❌ | ❌ |
| Bathroom coverage | ✅ IP65 option | ✅ Wearable | ❌ No one wants cameras | ❌ |
| Works without WiFi | ✅ Local mesh | ✅ Cellular | ❌ | ✅ Cellular |
| Two-way voice | ✅ Hub speaker + mic | ✅ Pendant speaker | ⚠️ Camera speaker | ✅ Base station |
| Cost (hardware) | ~$165 | $50-100 | $100-200/room | $300+ |
| Monthly fee | $0 (self-hosted) | $15-30/mo | Cloud storage fees | $30-50/mo |
| Open source | ✅ Full | ❌ | ❌ | ❌ |

---

## Getting Started

### Hardware Assembly
1. Assemble hub node PCB, flash firmware via SWD
2. Assemble room monitor PCBs, flash firmware via SWD
3. Assemble bed mat (solder FSRs to flexible PCB, flash firmware)
4. Assemble wearable tags, flash firmware via SWD
5. Print enclosures (FDM or resin recommended)
6. Install: mount room monitors at 1.5-2m height, 30° down angle
7. Place bed mat under mattress (between mattress and box spring)
8. Place hub in central location (living room recommended)
9. Pair all nodes via setup button (hold 5s on hub, then on each node)

### Software Setup
1. Deploy cloud backend: `docker-compose up -d` (Postgres + Mosquitto + FastAPI)
2. Connect hub to WiFi via mobile app or USB-C serial
3. Configure rooms and occupants in mobile app
4. Calibrate radar: walk through each room once for self-calibration
5. Calibrate bed mat: lie on bed for 30 seconds for baseline
6. Set caregiver contacts and alert preferences

### ML Model Training
1. Collect 2 weeks of baseline data
2. Train personalization models: `python train_anomaly.py --home-id <ID>`
3. Deploy models to hub: OTA update via dashboard

---

## License

MIT — build it, sell it, improve it. Keep elders safe.

---

*Invented and maintained by [jayis1](https://github.com/jayis1). New system every 24h.*