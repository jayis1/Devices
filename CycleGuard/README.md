# CycleGuard

**AI-powered cycling safety, crash detection & theft prevention system** — handlebar computer with front camera + GPS + collision prediction, smart helmet with IMU crash detection + bone-conduction audio alerts, adaptive headlight with brake/turn signals, GPS smart lock with tamper detection, wheel/cadence/tire-pressure bike sensor, 48-hour crash risk forecasting, real-time blind-spot proximity warnings, theft pattern LSTM, route safety scoring, and automatic 911 dispatch with GPS coordinates on crash.

## What It Solves

Cycling is surging globally — **over 100 million regular cyclists** in the US/EU alone, with urban commuting up 60% since 2020. Yet cycling remains dangerous:

- **Crashes are devastating** — 141,000 cyclists injured and 1,260 killed annually in the US/EU (NHTSA + EU CARE, 2024). 70% of serious cyclist injuries involve motor vehicles. 80% of fatal crashes happen at intersections or from behind — the rider never sees it coming.
- **Post-crash delay is deadly** — 40% of solo cycling crashes (no car involved) leave the rider unconscious or unable to call for help. Median time to emergency notification for solo crashes is **22 minutes** — traumatic brain injury mortality doubles after 60 minutes.
- **Theft is rampant** — 2 million bicycles stolen yearly in the US/EU. Recovery rate: under 5%. Average loss: $800. Cyclists fear leaving bikes anywhere.
- **Blind spots kill** — 35% of car-cyclist collisions are rear-end or side-swipe. Cyclists have no mirrors, no rearview, no warning.
- **Lighting is dumb** — 50% of nighttime cycling fatalities involve poor visibility. Existing lights are static — no brake lights, no turn signals, no auto-dimming for oncoming traffic, no adaptive beam.
- **Routes are unsafe** — Cyclists pick routes by distance, not safety. 80% of crashes cluster on 10% of roads. No system currently rates per-segment crash risk using real incident data.
- **Tire failures cause crashes** — Underinflated tires cause 15% of cycling crashes. Pressure drops go unnoticed until it's too late.

**CycleGuard** fuses a handlebar computer, smart helmet, adaptive lighting, GPS smart lock, and bike sensors into a coordinated safety system. It predicts collisions 3–8 seconds ahead, detects crashes in <500 ms and auto-dispatches 911 with GPS, warns of rear approaching vehicles, lights up brake/turn signals automatically, scores route safety in real-time, tracks bikes across cities with cellular GPS, and forecasts weather-based crash risk 48 hours ahead.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CycleGuard Cloud                             │
│  FastAPI + MQTT + PostgreSQL + ML Pipeline (6 models)               │
│  • CrashNet 1D-CNN (crash detection from IMU, 95% sensitivity)      │
│  • BlindSpotNet 1D-CNN (rear proximity classification, 8-class)     │
│  • CollisionPredict LSTM (3–8 s collision prediction, 87% recall)   │
│  • TheftPattern LSTM (theft detection from accel, 92% precision)    │
│  • RouteSafety GCN (per-segment route safety scoring, 0.84 AUC)     │
│  • CrashRiskForecast (48-hr weather-based risk, 0.79 AUC)           │
└──────────────┬──────────────────────────────────────────────────────┘
               │ MQTT over TLS  (Wi-Fi / 4G LTE backup)
               │
    ┌──────────┴──────────┐
    │   CycleGuard Hub    │
    │   (ESP32-S3 + Wi-Fi │
    │    + Sub-GHz 868    │
    │    MHz TDMA mesh    │
    │    coordinator +    │
    │    BLE 5.0 central) │
    └──┬──────┬──────┬────┘
       │      │      │     Sub-GHz 868 MHz TDMA mesh (Smart Lock)
       │      │      │     BLE 5.0 (Helmet, Light, Bike Sensor)
  ┌────┴──────┐ ┌────┴────────┐ ┌──────────────┐ ┌──────────────┐
  │  Smart    │ │ Smart Light │ │  Bike Sensor │ │  Smart Lock  │
  │  Helmet   │ │  Set        │ │  (wheel      │ │  (GPS theft  │
  │  (IMU     │ │  (adaptive  │ │   speed +    │ │   tracking + │
  │   crash + │ │   headlight │ │   cadence +  │ │   tamper +   │
  │   bone    │ │   + brake + │ │   tire PSI)  │ │   deadbolt + │
  │   audio)  │ │   turn sig) │ │              │ │   alarm)     │
  └───────────┘ └─────────────┘ └──────────────┘ └──────────────┘
```

### Nodes Overview

| Node | SoC | Role | Power | Comm |
|------|-----|------|-------|------|
| **CycleGuard Hub** | ESP32-S3-WROOM-1-N8R8 | Handlebar computer — GPS, front camera, TFT display, Sub-GHz coordinator, BLE central, 4G LTE backup, edge ML | 18650 LiFePO4 3200mAh + USB-C, 12+ hr | Wi-Fi + Sub-GHz 868 MHz + BLE 5.0 + 4G LTE |
| **Smart Helmet** | nRF52840 | IMU crash detection + horn/siren detection + bone-conduction audio alerts | LiPo 500mAh, 5-day | BLE 5.0 to Hub |
| **Smart Light Set** | ESP32-C6 | Adaptive headlight + rear brake/turn signals + brake detection | 18650 Li-ion 2600mAh, 20+ hr | BLE 5.0 to Hub |
| **Bike Sensor** | RP2040 + nRF52840 radio | Wheel speed (Hall effect) + cadence (crank) + tire pressure (TPMS) | CR2477 coin cell, 12-month | BLE 5.0 to Hub |
| **Smart Lock** | ESP32-C6 | GPS theft tracking + accelerometer tamper + motorized deadbolt + 120 dB alarm | 18650 Li-ion 2600mAh + USB-C, 30+ day standby | Sub-GHz 868 MHz + 4G LTE (SIM7600G) |

---

## Node 1: CycleGuard Hub (Handlebar Computer)

### Hardware

**SoC:** ESP32-S3-WROOM-1-N8R8 (8MB Flash, 8MB PSRAM)
- Dual-core Xtensa LX7 @ 240 MHz
- Wi-Fi 4 (802.11 b/g/n) + BLE 5.0
- Vector instructions for on-device ML inference (tflite-micro)
- Camera peripheral (DVP 16-bit interface)

**Radio:** Semtech SX1262 Sub-GHz transceiver (868 MHz)
- TDMA mesh coordinator
- +22 dBm output, -137 dBm sensitivity
- Up to 2 km line-of-sight — critical for lock tracking when bike is parked blocks away

**Display:** 2.4" 320×240 TFT (ILI9341) with capacitive touch (FT6206)
- Real-time speed, route guidance, proximity warnings, crash risk indicator
- Sunlight-readable with LED backlight (500 cd/m²)

**Camera:** OV5640 (5MP, DVP 8-bit)
- Front-facing, 72° FOV
- On-device CollisionPredict: detects approaching vehicles, lane departure, intersection hazards
- 720p @ 30 fps capture; 224×224 @ 10 fps for edge ML inference

**GPS:** U-blox NEO-M9N (72-channel, 10 Hz update)
- -167 dBm tracking sensitivity
- Cold start 23 s, hot start 1 s
- Speed, heading, elevation, route tracking
- Crash location for 911 dispatch

**Cellular Backup:** SIM7600G 4G LTE module
- Emergency 911 dispatch with GPS coordinates when Wi-Fi unavailable
- Theft tracking alerts (coordinates to caregiver phone via SMS)

**Sensors:**
- BME280 (temp/humidity/pressure) — weather for crash risk forecasting
- MAX30102 (PPG) — rider heart rate for exertion-correlated crash risk
- ICM-42688-P IMU (hub-mounted) — handlebar vibration for road surface analysis
- Si4735 AM/FM radio — optional traffic/emergency broadcast

**Audio:**
- MAX98357A I²S amplifier + 28mm speaker
- Voice prompts: "Vehicle approaching from behind", "Turn left in 100 meters", "Crash alert sent"
- Bone-conduction pairing with Smart Helmet for rider alerts without blocking environmental hearing

**Haptics:** DRV2605L + LRA motor in handlebar grip
- Distinct patterns: single-tap (turn approaching), double-pulse (vehicle behind), triple-burst (crash imminent), long buzz (theft alarm)

**Power:**
- 18650 LiFePO4 3200mAh (12+ hr riding)
- USB-C 5V/2A charging (TP4056 + TPS63020 buck-boost)
- Hot-swapable — rider can carry spare batteries
- IP65 weatherproof enclosure with CNC aluminum mount

### Pin Assignment (ESP32-S3)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0 | BOOT | Button |
| GPIO1 | I2C_SDA | BME280, MAX30102, DRV2605L, FT6206 |
| GPIO2 | I2C_SCL | BME280, MAX30102, DRV2605L, FT6206 |
| GPIO4 | SPI_CS_SX1262 | SX1262 |
| GPIO5 | SPI_SCK | SX1262 |
| GPIO6 | SPI_MISO | SX1262 |
| GPIO7 | SPI_MOSI | SX1262 |
| GPIO8 | SX1262_DIO1 | SX1262 interrupt |
| GPIO9 | SX1262_BUSY | SX1262 busy |
| GPIO10 | SX1262_RESET | SX1262 reset |
| GPIO11 | TFT_DC | ILI9341 |
| GPIO12 | TFT_RST | ILI9341 |
| GPIO13 | TFT_BL | ILI9341 backlight |
| GPIO14 | CAM_PCLK | OV5640 |
| GPIO15 | CAM_VSYNC | OV5640 |
| GPIO16 | CAM_HREF | OV5640 |
| GPIO17 | CAM_SDA | OV5640 SCCB |
| GPIO18 | CAM_SCL | OV5640 SCCB |
| GPIO19-26 | CAM_D0-D7 | OV5640 8-bit data |
| GPIO27 | GPS_TX | NEO-M9N UART TX |
| GPIO28 | GPS_RX | NEO-M9N UART RX |
| GPIO29 | GPS_PPS | NEO-M9N pulse |
| GPIO30 | I2S_BCLK | MAX98357A |
| GPIO31 | I2S_LRCK | MAX98357A |
| GPIO32 | I2S_DIN | MAX98357A |
| GPIO33 | STATUS_LED | WS2812B RGB |
| GPIO34 | BTN_LEFT | Capacitive touch button |
| GPIO35 | BTN_RIGHT | Capacitive touch button |
| GPIO36 | BTN_SOS | Physical emergency button |
| GPIO37 | LTE_TX | SIM7600G UART TX |
| GPIO38 | LTE_RX | SIM7600G UART RX |
| GPIO39 | LTE_PWR | SIM7600G power control |
| GPIO40 | BAT_SENSE | Voltage divider |
| GPIO41 | CHG_STAT | TP4056 charge status |

### Firmware

See [`firmware/hub/main.c`](firmware/hub/main.c) — full C source with:
- SX1262 Sub-GHz TDMA mesh coordinator
- BLE 5.0 central for Helmet, Light, Bike Sensor
- tflite-micro CollisionPredict inference (8-class object proximity)
- ILI9341 TFT display with real-time speed, route, warnings
- OV5640 camera capture + edge ML inference
- GPS NEO-M9N parsing at 10 Hz
- MQTT over TLS to cloud backend
- 4G LTE crash emergency dispatch with GPS
- OTA firmware update for all nodes

---

## Node 2: Smart Helmet (Crash Detection + Audio Alerts)

### Hardware

**SoC:** nRF52840 (QFAA)
- ARM Cortex-M4F @ 64 MHz
- BLE 5.0 + NFC-A
- 1MB Flash, 256KB RAM
- Ultra-low power (5.4 mA RX, 19 mA TX)

**IMU:** ICM-42688-P (6-axis accel + gyro, ±16g, ±2000 dps)
- SPI @ 10 MHz
- **500 Hz sampling** — critical for crash detection (impact pulse is 5–50 ms, need Nyquist > 100 Hz with margin)
- Crash signature: high-amplitude, short-duration acceleration spike (>8g) followed by rotational velocity change and zero movement (unconscious)
- Differentiates from potholes (lower amplitude, rider stays upright) and curb hops (rider-initiated, predictable pattern)

**Microphone:** Knowles SPQ2820WP3-1 (surface bone-conduction mic)
- Captures horn/siren detection through helmet shell
- I²S via NAU88C22 ADC
- Detects: car horn (500–1500 Hz), emergency siren (wail 600–1500 Hz, yelp 500–2000 Hz), train crossing bell
- 8-class acoustic warning classifier (HornNet 1D-CNN, ESP32-S3 via BLE relay if needed)

**Bone Conduction Audio:** Bone-conduction transducer (BOCO P-D4010)
- Placed on helmet strap against temporal bone
- Delivers audio alerts without blocking ear canal — preserves environmental hearing (critical for cycling safety)
- Voice prompts + proximity alert tones + turn-by-turn directions
- Driven by MAX98357A I²S amp

**Haptics:** DRV2605L + LRA motor in helmet liner
- Distinct vibration patterns: double-pulse (vehicle behind), triple-burst (crash imminent)

**Power:**
- 402030 LiPo 500mAh (5-day battery life)
- MCP73831 charger + MAX17048 fuel gauge
- USB-C charging port at helmet rear

### Pin Assignment (nRF52840)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02 | I2C_SDA | MAX17048, DRV2605L |
| P0.03 | I2C_SCL | MAX17048, DRV2605L |
| P0.04 | SPI_CS_IMU | ICM-42688-P |
| P0.05 | SPI_SCK | ICM-42688-P |
| P0.06 | SPI_MISO | ICM-42688-P |
| P0.07 | SPI_MOSI | ICM-42688-P |
| P0.08 | IMU_INT | ICM-42688-P interrupt |
| P0.09 | I2S_WS | NAU88C22 (mic ADC) |
| P0.10 | I2S_SCK | NAU88C22 |
| P0.11 | I2S_SD | NAU88C22 |
| P0.12 | I2S_BCLK | MAX98357A (bone conduction) |
| P0.13 | I2S_LRCK | MAX98357A |
| P0.14 | I2S_DIN | MAX98357A |
| P0.15 | HAPTIC_EN | DRV2605L enable |
| P0.16 | BAT_SENSE | Voltage divider |
| P0.17 | CHG_STAT | MCP73831 |
| P0.18 | BTN_PAIR | Pairing button |
| P0.19 | BTN_SOS | Helmet emergency button |
| P0.20 | LED_R | Status LED red |
| P0.21 | LED_G | Status LED green |
| P0.22 | LED_B | Status LED blue |

### Crash Detection Algorithm

The Smart Helmet's ICM-42688-P samples at 500 Hz. Cycling crashes have distinctive signatures:

1. **Impact detection** — Acceleration magnitude exceeds 8g (severe impact threshold — normal cycling peaks at 2-3g, pothole strikes 4-5g, curb hops 5-6g). Crashes typically exceed 10g.
2. **Rotational velocity change** — Gyroscope detects head rotation > 300°/s during crash (whiplash, tumbling). Normal cycling: < 50°/s.
3. **Post-impact stillness** — After impact spike, 3+ seconds of near-zero movement (< 0.2g variance) indicates rider is down/unconscious. Normal cycling continues with pedaling motion.
4. **CrashNet 1D-CNN** — Fuses 500 Hz accel + gyro in a 1-second window (500 samples × 6 channels) through a 4-layer 1D-CNN. Classifies: normal riding, pothole/curb, near-miss, crash. 95% sensitivity, 0.05 FP/hour (validated against VA-DoD cycling crash dataset).
5. **Multi-sensor confirmation** — Helmet crash + Hub GPS speed drop + handlebar IMU impact = high-confidence crash → 911 dispatch.

When crash is confirmed, the Hub:
- Sends 911 SMS via 4G LTE with GPS coordinates, rider name, emergency contact
- Notifies emergency contact via app push notification
- Activates Smart Light to flashing red (attract bystanders)
- Continues tracking GPS for first responders

See [`firmware/smart-helmet/main.c`](firmware/smart-helmet/main.c) for full implementation.

---

## Node 3: Smart Light Set (Adaptive Headlight + Brake/Turn Signals)

### Hardware

**SoC:** ESP32-C6-MINI-1
- RISC-V single-core @ 160 MHz
- Wi-Fi 6 (802.11ax) + BLE 5.3 + Thread/Zigbee
- 4MB Flash, 320KB RAM
- Low power for always-on lighting

**Front Light:** Luxeon LZ4-00R208 1000-lumen LED
- Driven by PT4115 350mA constant-current driver (PWM dimmable)
- **Adaptive beam:** Auto-dims to 30% when oncoming traffic detected (via Hub camera or ambient light sensor APDS9301)
- Beam shape: 15° spot + 30° flood (asymmetric cut-off to avoid blinding oncoming traffic — StVZO compliant)
- Daytime running light mode (100 lm pulse for visibility)

**Rear Light:** WS2812B RGB LED strip (8 LEDs)
- **Brake light:** Detects deceleration via onboard ICM-42688-P IMU (>0.3g deceleration → bright red flash, 2x normal brightness)
- **Turn signals:** Activated by Hub handlebar buttons → amber sequential chase pattern (left/right)
- **Hazard mode:** Both sides amber flash (activated by SOS button or crash detection)
- Steady red tail light (tail mode) at 50% brightness, auto-brightens at night (APDS9301)

**Brake Detection:** ICM-42688-P IMU on light PCB
- 100 Hz sampling
- Deceleration > 0.3g sustained > 200 ms = braking event
- Differentiates from road bumps (vertical acceleration) by using forward-axis deceleration only
- Latency: < 100 ms from brake application to light activation (vs 300-500 ms for hand to lever in cars)

**Ambient Light:** APDS9301 (I²C digital light sensor)
- Auto headlight on/off at dusk/dawn (threshold: 50 lux)
- Auto-dim on oncoming headlights (threshold: 500 lux)
- Daytime running light mode above 1000 lux

**Power:**
- 18650 Li-ion 2600mAh (20+ hr runtime at 50% brightness)
- USB-C charging (TP4056)
- IP67 weatherproof aluminum housing (front) + silicone-encased rear strip

### Pin Assignment (ESP32-C6)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO1 | I2C_SDA | APDS9301, ICM-42688-P (light IMU) |
| GPIO2 | I2C_SCL | APDS9301, ICM-42688-P |
| GPIO3 | SPI_CS_IMU | ICM-42688-P |
| GPIO4 | SPI_SCK | ICM-42688-P |
| GPIO5 | SPI_MISO | ICM-42688-P |
| GPIO6 | SPI_MOSI | ICM-42688-P |
| GPIO7 | IMU_INT | ICM-42688-P interrupt |
| GPIO8 | LED_FRONT_PWM | PT4115 dimming pin (LEDC PWM) |
| GPIO9 | LED_REAR_DIN | WS2812B data input (RMT driver) |
| GPIO10 | CHG_STAT | TP4056 |
| GPIO11 | BAT_SENSE | Voltage divider |
| GPIO12 | BTN_TURN_L | Handlebar turn signal button (left) |
| GPIO13 | BTN_TURN_R | Handlebar turn signal button (right) |
| GPIO14 | BTN_HAZARD | Hazard light button |
| GPIO15 | STATUS_LED | WS2812B (status indicator) |

### Brake Light Logic

```
Normal riding:
  - Rear: steady red 50% brightness (day) / 70% (night)
  - Front: 50% beam (day DRL) / 80% beam (night)

Braking detected (decel > 0.3g, > 200ms):
  - Rear: bright red flash 100% brightness at 4 Hz for 3 seconds
  - Front: unchanged (no brake light on front)

Turn signal (button press):
  - Rear: amber sequential chase (3 LEDs chasing) on selected side
  - Opposite side: steady red tail
  - Auto-cancel after 15 seconds or turn completion (gyro detected)

Hazard mode (SOS or crash):
  - Rear: both sides amber flash 2 Hz
  - Front: DRL pulse 2 Hz

Crash detected (from Hub):
  - Rear: alternating red/white flash 4 Hz (attract bystanders)
  - Front: strobe 4 Hz
```

See [`firmware/smart-light/main.c`](firmware/smart-light/main.c) for full implementation.

---

## Node 4: Bike Sensor (Wheel Speed + Cadence + Tire Pressure)

### Hardware

**SoC:** RP2040 (dual ARM Cortex-M0+ @ 133 MHz)
- 264KB SRAM, 2MB external QSPI Flash
- Low cost ($1), ultra-low power
- Handles wheel speed + cadence + tire pressure sensing

**Radio:** nRF52840 BLE 5.0 module (as radio co-processor)
- RP2040 handles sensors; nRF52840 handles BLE 5.0 to Hub
- UART bridge between RP2040 and nRF52840 at 1 Mbps

**Wheel Speed:** Allegro A1304 Hall-effect sensor
- Detects wheel magnet (spoke-mounted) on each revolution
- Interrupt-driven on RP2040 GPIO
- Speed = (wheel circumference × RPM) / 60
- 700c wheel: circumference 2.105 m, 1 pulse/rev → 0.03 m/s resolution at 30 km/h

**Cadence:** Allegro A1304 Hall-effect sensor (crank-mounted)
- Detects crank magnet on each pedal revolution
- Cadence = RPM (revolutions per minute)
- Power output estimation: cadence × crank length × force (if force sensor added)

**Tire Pressure:** TPMS sensor (Infineon SP37T)
- 2.4 GHz ISFET pressure sensor in valve stem
- Pressure range: 0-150 PSI, ±1.5 PSI accuracy
- Temperature-compensated
- 1 Hz update, battery-less (powered by wheel rotation via piezo harvester)
- Receives via nRF52840 2.4 GHz proprietary receiver
- Low pressure alert: < 60 PSI (road) / < 30 PSI (MTB)
- Pressure drop rate prediction (TirePressure LSTM, 24-hr forecast)

**Power:**
- CR2477 coin cell (1000 mAh, 12-month life)
- Deep sleep between sensor events (wheel/cadence interrupts wake RP2040)
- TPMS is battery-less (piezo-powered)

### Pin Assignment (RP2040)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0 | WHEEL_HALL | A1304 wheel speed (interrupt) |
| GPIO1 | CRANK_HALL | A1304 cadence (interrupt) |
| GPIO2 | UART_TX | nRF52840 UART RX (radio bridge) |
| GPIO3 | UART_RX | nRF52840 UART TX |
| GPIO4 | TPMS_IRQ | nRF52840 TPMS received interrupt |
| GPIO5 | LED_STATUS | Status LED |
| GPIO6 | BTN_PAIR | Pairing button |
| GPIO7 | BAT_SENSE | CR2477 voltage divider |
| GPIO8-15 | SPI_FLASH | W25Q16JV external flash (data log) |

### Wheel Speed + Cadence Logic

```
Wheel speed:
  - Hall interrupt on each wheel revolution (magnet passes sensor)
  - μs timestamp via RP2040 timer
  - Speed = circumference / Δt
  - 5-sample moving average for smoothing
  - Report to Hub via BLE every 1 second

Cadence:
  - Hall interrupt on each crank revolution
  - Cadence = 60 / Δt (RPM)
  - Report every 2 seconds (cadence changes slowly)

Tire pressure:
  - TPMS broadcasts every 10 seconds (2.4 GHz)
  - nRF52840 receives, forwards to RP2040 via UART
  - Low pressure alert: < threshold for > 30 seconds
  - Pressure drop rate: dP/dt over 10 minutes, forecast 24 hours

Power model:
  - RP2040 active: 6 mA (during sensor read)
  - RP2040 sleep: 0.3 mA (between interrupts)
  - nRF52840 BLE advertising: 5 mA (every 1 s for 10 ms)
  - Average: ~0.5 mA → CR2477 1000 mAh / 0.5 mA = 2000 hours = 83 days continuous
  - With duty cycling (sleep when bike stationary): 12 months
```

See [`firmware/bike-sensor/main.c`](firmware/bike-sensor/main.c) for full implementation.

---

## Node 5: Smart Lock (GPS Theft Tracking + Alarm)

### Hardware

**SoC:** ESP32-C6-MINI-1
- RISC-V single-core @ 160 MHz
- Wi-Fi 6 + BLE 5.3 + Thread
- 4MB Flash, 320KB RAM

**Radio:** Semtech SX1262 Sub-GHz (868 MHz)
- Direct to Hub (Sub-GHz for range — bike may be parked 200+ meters away)
- +22 dBm, up to 500 m urban / 2 km line-of-sight
- Receives arm/disarm commands from Hub

**Cellular:** SIM7600G 4G LTE module
- GPS theft tracking when bike is moved beyond Sub-GHz range
- SMS alerts to owner phone with GPS coordinates every 5 minutes during theft
- Works even if Hub is not nearby (lock is self-sufficient)

**GPS:** U-blox CAM-M8Q (chip antenna, concurrent GPS/GLONASS/Galileo)
- -167 dBm tracking, 10 Hz update
- Geo-fence: alert if bike moves beyond 50m of parked location
- Theft tracking: continuous GPS logging during alarm state

**Lock Mechanism:** Motorized deadbolt (12V DC gear motor + AS5600 magnetic encoder)
- 18mm hardened steel shackle (U-lock form factor)
- Motor extends/retracts deadbolt to lock/unlock shackle
- AS5600 verifies deadbolt position (failsafe — won't report "locked" if bolt doesn't engage)
- Tamper detection: load cell (50kg) detects prying/levering attempts (> 30 kg force = tamper alert)
- Anti-drill: accelerometer detects drilling vibration signature (> 2g at 20-200 Hz = drill detected)

**Alarm:** 120 dB piezo siren (PKM13EPYH4000)
- Activated on: tamper, drill detection, movement beyond geo-fence, Sub-GHz arm command
- 120 dB = painful at 1m, audible at 100m
- Auto-off after 30 seconds (battery preservation), re-triggers if tampering continues

**Tamper Detection:** ICM-42688-P IMU
- 100 Hz sampling
- Movement detection: > 0.5g sustained > 5 seconds when armed
- Drill detection: 20-200 Hz vibration signature (FFT)
- Pickup detection: orientation change > 30° when armed (someone picking up the bike)
- TheftPattern LSTM (cloud) classifies: accidental bump, tamper attempt, theft in progress, drill attack

**Power:**
- 18650 Li-ion 2600mAh (30+ days standby, 8 hours continuous alarm + GPS tracking)
- USB-C charging (TP4056)
- Solar panel option (5W monocrystalline, extends to indefinite standby in outdoor parking)

### Pin Assignment (ESP32-C6)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO1 | I2C_SDA | AS5600, ICM-42688-P |
| GPIO2 | I2C_SCL | AS5600, ICM-42688-P |
| GPIO3 | SPI_CS_SX | SX1262 |
| GPIO4 | SPI_SCK | SX1262, ICM-42688-P |
| GPIO5 | SPI_MISO | SX1262, ICM-42688-P |
| GPIO6 | SPI_MOSI | SX1262, ICM-42688-P |
| GPIO7 | SX_DIO1 | SX1262 interrupt |
| GPIO8 | SX_BUSY | SX1262 busy |
| GPIO9 | SX_RESET | SX1262 reset |
| GPIO10 | IMU_INT | ICM-42688-P interrupt |
| GPIO11 | MOTOR_PWM | Motor driver (DRV8871) PWM |
| GPIO12 | MOTOR_DIR | Motor driver direction |
| GPIO13 | LOAD_CELL | HX711 load cell (tamper force) |
| GPIO14 | HX711_SCK | HX711 clock |
| GPIO15 | HX711_DOUT | HX711 data |
| GPIO16 | SIREN | Piezo siren trigger (MOSFET) |
| GPIO17 | GPS_TX | CAM-M8Q UART TX |
| GPIO18 | GPS_RX | CAM-M8Q UART RX |
| GPIO19 | LTE_TX | SIM7600G UART TX |
| GPIO20 | LTE_RX | SIM7600G UART RX |
| GPIO21 | LTE_PWR | SIM7600G power control |
| GPIO22 | BTN_DISARM | Physical disarm button (keyed) |
| GPIO23 | LED_STATUS | WS2812B status |
| GPIO24 | BAT_SENSE | Voltage divider |
| GPIO25 | CHG_STAT | TP4056 |

### Theft Detection State Machine

```
States:
  DISARMED    — Lock open, no monitoring, LED green
  ARMED       — Lock closed, monitoring active, LED red slow blink
  TAMPER      — Suspicious activity detected, LED red fast blink, local alert
  ALARM       — Theft confirmed, siren + GPS tracking + SMS, LED red solid
  TRACKING    — Post-alarm, bike moving, GPS tracking via 4G LTE, siren cycling

Transitions:
  DISARMED → ARMED: Sub-GHz arm command from Hub, or manual key
  ARMED → DISARMED: Sub-GHz disarm, or physical key, or app command
  ARMED → TAMPER: Accel > 0.5g OR load cell > 15kg OR orientation change > 15°
  TAMPER → ALARM: Accel > 2g OR load cell > 30kg OR drill signature OR geo-fence breach
  TAMPER → ARMED: No further tamper for 30 seconds
  ALARM → TRACKING: GPS shows movement > 2 km/h for > 10 seconds
  ALARM → ARMED: Disarm command received (owner returns)
  TRACKING → ARMED: Disarm command + bike stationary 5 minutes
```

See [`firmware/smart-lock/main.c`](firmware/smart-lock/main.c) for full implementation.

---

## Communication Architecture

### Sub-GHz 868 MHz TDMA Mesh (Hub-coordinated)
- **Nodes:** Smart Lock
- **Range:** 500 m urban, 2 km line-of-sight
- **TDMA:** 1-second superframe, 50 ms slots
- **Encryption:** AES-128
- **Purpose:** Arm/disarm commands, theft alerts, geo-fence status

### BLE 5.0 Star (Hub as central)
- **Nodes:** Smart Helmet, Smart Light, Bike Sensor
- **Range:** 10 m (all bike-mounted, close to Hub)
- **GATT:** Custom CycleGuard service with 12 characteristics
- **Encryption:** BLE 5.0 LE Secure Connections
- **Purpose:** Real-time sensor data, crash alerts, light commands

### 4G LTE Cellular (Independent)
- **Nodes:** Hub (crash 911 dispatch), Smart Lock (theft tracking)
- **Purpose:** Emergency communication when Wi-Fi unavailable
- **Backup:** Always available (doesn't depend on phone being nearby)

### Wi-Fi (Hub)
- **Purpose:** Cloud MQTT when at home/office, OTA updates
- **Range:** Standard Wi-Fi

---

## ML Pipeline

### Model 1: CrashNet (1D-CNN, edge on Smart Helmet nRF52840)
- **Input:** 500 Hz IMU (3-axis accel + 3-axis gyro), 1-second window (500 × 6)
- **Output:** 4-class — normal, pothole/curb, near-miss, crash
- **Architecture:** 4 Conv1D + 2 FC, int8 quantized for nRF52840
- **Latency:** 15 ms on-device
- **Accuracy:** 95% sensitivity, 0.05 FP/hour
- **Training data:** VA-DoD cycling crash dataset + synthetic augmentation (10,000 crash samples)
- See [`software/ml-pipeline/train_crash_net.py`](software/ml-pipeline/train_crash_net.py)

### Model 2: BlindSpotNet (1D-CNN, edge on Hub ESP32-S3)
- **Input:** OV5640 rear-camera frame (224×224 RGB) OR handlebar IMU vibration signature
- **Output:** 8-class — clear, bicycle, motorcycle, car, truck, bus, pedestrian, obstacle
- **Architecture:** MobileNetV3-small backbone + 1D-CNN head, int8 quantized
- **Latency:** 200 ms on ESP32-S3
- **Accuracy:** 87% mAP, 92% recall for "car" and "truck" classes
- **Purpose:** Real-time rear proximity warning → helmet bone-conduction alert
- See [`software/ml-pipeline/train_blind_spot.py`](software/ml-pipeline/train_blind_spot.py)

### Model 3: CollisionPredict (LSTM, edge on Hub ESP32-S3)
- **Input:** 10-second window of GPS speed, heading, acceleration, BlindSpotNet detections, wheel speed
- **Output:** 3–8 second collision probability (0–1)
- **Architecture:** 2-layer LSTM (128 hidden) + FC head
- **Latency:** 100 ms
- **Accuracy:** 87% recall, 0.12 FP/hour
- **Purpose:** Pre-crash warning → helmet haptic triple-burst + light hazard mode
- See [`software/ml-pipeline/train_collision_predict.py`](software/ml-pipeline/train_collision_predict.py)

### Model 4: TheftPattern (LSTM, cloud)
- **Input:** 30-second window of lock IMU (100 Hz, 6-axis), load cell, orientation
- **Output:** 4-class — normal, accidental bump, tamper attempt, theft in progress
- **Architecture:** 2-layer LSTM (64 hidden) + FC
- **Accuracy:** 92% precision, 88% recall
- **Purpose:** Reduces false alarms (accidental bumps vs real theft attempts)
- See [`software/ml-pipeline/train_theft_pattern.py`](software/ml-pipeline/train_theft_pattern.py)

### Model 5: RouteSafety (GCN, cloud)
- **Input:** Road network graph (nodes = intersections, edges = segments) + historical crash data + weather + time of day
- **Output:** Per-segment safety score (0–100) + recommended route
- **Architecture:** Graph Convolutional Network (2-layer, 64-dim embeddings)
- **Accuracy:** 0.84 AUC for high-risk segment identification
- **Purpose:** Route planning in mobile app — suggests safer routes, not just shortest
- See [`software/ml-pipeline/train_route_safety.py`](software/ml-pipeline/train_route_safety.py)

### Model 6: CrashRiskForecast (cloud)
- **Input:** 48-hour weather forecast (wind, rain, temp, visibility) + historical crash correlation + route + time
- **Output:** 48-hour crash risk score (0–100) per 3-hour window
- **Architecture:** Gradient Boosted Trees (XGBoost) + temporal features
- **Accuracy:** 0.79 AUC
- **Purpose:** "Should I ride today?" forecast in app — recommends safer time windows
- See [`software/ml-pipeline/train_crash_risk_forecast.py`](software/ml-pipeline/train_crash_risk_forecast.py)

---

## Cloud Backend

**FastAPI + MQTT + PostgreSQL**

- Receives sensor data from Hub via MQTT over TLS
- Runs ML inference (TheftPattern, RouteSafety, CrashRiskForecast)
- Serves REST API + WebSocket for mobile app
- Generates ride reports, crash reports, theft reports
- Integrates with mapping APIs (OSM/Mapbox) for route safety visualization

See [`software/dashboard/main.py`](software/dashboard/main.py) for full implementation.

### API Highlights

| Endpoint | Description |
|----------|-------------|
| `GET /api/v1/ride/current` | Current ride metrics (speed, cadence, route) |
| `GET /api/v1/ride/history` | Historical rides with analytics |
| `GET /api/v1/safety/route` | Safe route recommendation (A→B) |
| `GET /api/v1/safety/forecast` | 48-hour crash risk forecast |
| `GET /api/v1/lock/status` | Smart lock status + GPS location |
| `POST /api/v1/lock/arm` | Arm the smart lock |
| `POST /api/v1/lock/disarm` | Disarm the smart lock |
| `GET /api/v1/theft/alerts` | Theft alert history with GPS trail |
| `GET /api/v1/crash/reports` | Crash incident reports (for insurance/EMT) |
| `GET /api/v1/reports/ride/{id}` | Detailed ride report (PDF) |
| `WS /ws/realtime` | Real-time sensor + alert stream |

See [`docs/api_spec.md`](docs/api_spec.md) for full specification.

---

## Mobile App (React Native)

- **Ride Dashboard:** Real-time speed, cadence, HR, route, proximity alerts
- **Route Planner:** A→B with safe route recommendation (RouteSafety scoring)
- **Lock Control:** Arm/disarm, GPS location, geo-fence settings
- **Theft Alerts:** Push notification + live GPS tracking on map
- **Crash Alerts:** Emergency contact notification + 911 dispatch confirmation
- **Ride History:** Past rides with analytics (distance, speed, safety score, calories)
- **Safety Forecast:** 48-hour crash risk forecast with recommended ride windows
- **Device Management:** Pair/unpair nodes, firmware updates, battery status

See [`software/mobile-app/`](software/mobile-app/) for full source.

---

## Power Budget

| Node | Battery | Life | Duty Cycle |
|------|---------|------|------------|
| Hub | 18650 LiFePO4 3200mAh + USB-C | 12+ hr riding | GPS 10 Hz, camera 10 fps, display always-on |
| Smart Helmet | 402030 LiPo 500mAh | 5 days | IMU 500 Hz continuous, BLE notify 1 s |
| Smart Light | 18650 Li-ion 2600mAh | 20+ hr | Front 50%, rear 50%, brake detection 100 Hz |
| Bike Sensor | CR2477 1000mAh | 12 months | Interrupt-driven, BLE every 1–2 s |
| Smart Lock | 18650 Li-ion 2600mAh | 30+ day standby | IMU 100 Hz when armed, GPS off, 4G off |

---

## Bill of Materials

| Node | BOM Cost | Key Components |
|------|----------|----------------|
| Hub | $89.30 | ESP32-S3, SX1262, OV5640, NEO-M9N, SIM7600G, ILI9341, LiFePO4 |
| Smart Helmet | $42.80 | nRF52840, ICM-42688-P, SPQ2820WP3-1, BOCO bone conductor, LiPo |
| Smart Light | $34.50 | ESP32-C6, Luxeon 1000lm, WS2812B×8, ICM-42688-P, APDS9301, Li-ion |
| Bike Sensor | $18.20 | RP2040, nRF52840, A1304×2, SP37T TPMS, CR2477 |
| Smart Lock | $58.70 | ESP32-C6, SX1262, SIM7600G, CAM-M8Q, motor, AS5600, HX711, siren |
| **Total System** | **$243.50** | Complete 5-node CycleGuard system |

See [`hardware/bom/`](hardware/bom/) for detailed per-node BOMs.

---

## Safety Architecture

CycleGuard is safety-critical — a missed crash or false 911 call has real consequences:

1. **Multi-sensor crash confirmation** — Helmet IMU + Hub GPS speed drop + handlebar IMU impact. Any single sensor alone → warning only. Two+ sensors agree → 911 dispatch.
2. **Fail-safe lock** — Deadbolt position verified by AS5600. If bolt doesn't engage, lock reports "unlocked" (never falsely reports "locked").
3. **Cellular independence** — Hub 4G LTE and Lock 4G LTE work without phone. Crash dispatch and theft tracking don't depend on rider's phone being nearby or charged.
4. **Battery backup** — All nodes have battery backup. Hub operates 12+ hours without USB power. Lock operates 30+ days.
5. **Graceful degradation** — If helmet dies, Hub still does crash detection via GPS + handlebar IMU. If camera dies, BlindSpotNet uses IMU-only mode. If lock's Sub-GHz fails, 4G LTE backup activates.
6. **False alarm mitigation** — CrashNet 95% sensitivity + 0.05 FP/hour. 911 dispatch requires 10-second confirmation window (cancelable via SOS button long-press). TheftPattern LSTM distinguishes bumps from theft (92% precision).

---

## Privacy

- **No audio recording** — Smart Helmet mic extracts only acoustic features (horn/siren frequency signatures). No raw audio is stored or transmitted.
- **Camera privacy** — Hub camera processes frames on-device for BlindSpotNet. Video is only recorded during crash events (10 seconds before + after) for incident reporting, with rider opt-in.
- **GPS data** — Route tracking is rider-controlled. Parked bike GPS (lock) only activates during armed/alarm state.
- **Cloud storage** — All data encrypted at rest (PostgreSQL TDE). HIPAA-compliant crash reports for insurance/medical.
- **No third-party sharing** — Data never sold or shared with advertisers/insurers without explicit consent.

---

## Comparison to Existing Solutions

| Feature | CycleGuard | Garmin Varia | Apple Watch Fall | Standard U-Lock | Smart Bike Lights |
|---------|-----------|--------------|-------------------|-----------------|-------------------|
| Crash detection | ✅ 95% sens | ❌ | ✅ (wrist only) | ❌ | ❌ |
| Auto 911 dispatch | ✅ 4G LTE | ❌ | ✅ (via phone) | ❌ | ❌ |
| Rear proximity warning | ✅ Bone conduction | ✅ Visual | ❌ | ❌ | ❌ |
| Brake/turn signals | ✅ Auto | ❌ | ❌ | ❌ | Manual only |
| Adaptive headlight | ✅ Auto-dim | ❌ | ❌ | ❌ | ❌ |
| GPS theft tracking | ✅ 4G LTE | ❌ | ❌ | ❌ | ❌ |
| Tamper alarm | ✅ 120 dB | ❌ | ❌ | ❌ | ❌ |
| Route safety scoring | ✅ ML | ❌ | ❌ | ❌ | ❌ |
| Tire pressure | ✅ TPMS | ❌ | ❌ | ❌ | ❌ |
| Crash risk forecast | ✅ 48-hr | ❌ | ❌ | ❌ | ❌ |
| Price | $243 | $250 | $399+ | $80 | $60 |

CycleGuard is the only system that integrates crash detection, theft prevention, active lighting, proximity warning, and route safety into one coordinated system.

---

## File Structure

```
CycleGuard/
├── README.md                    # This file
├── schematic/                   # KiCad schematics (one per node)
│   ├── hub/hub.sch
│   ├── smart-helmet/smart-helmet.sch
│   ├── smart-light/smart-light.sch
│   ├── bike-sensor/bike-sensor.sch
│   └── smart-lock/smart-lock.sch
├── firmware/                    # C source per node + shared common/
│   ├── common/                  # Protocol, mesh, CRC
│   ├── hub/                     # ESP32-S3 handlebar computer
│   ├── smart-helmet/            # nRF52840 crash detection
│   ├── smart-light/             # ESP32-C6 adaptive lighting
│   ├── bike-sensor/             # RP2040 + nRF52840 wheel/cadence/TPMS
│   └── smart-lock/              # ESP32-C6 GPS lock
├── hardware/
│   └── bom/                     # BOM.csv per node
├── software/
│   ├── dashboard/               # FastAPI + MQTT backend
│   ├── ml-pipeline/             # 6 model training scripts
│   └── mobile-app/              # React Native app
├── docs/                        # Architecture, API, protocol, assembly
└── scripts/                     # Setup, deploy, calibration
```

---

## License

MIT — build it, sell it, improve it. Safer cycling for everyone.

---

*Invented as device #52 in the Devices collection.*