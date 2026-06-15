# MedSync

**AI-powered medication adherence and health monitoring system.** Because 50% of people don't take their medications correctly — and that kills 125,000 Americans per year.

---

## What It Does

MedSync is a 4-node system that ensures the right person takes the right pill at the right time, every time:

1. **Dispenses** medications on schedule from a motorized, weight-verified pill carousel — no more missed doses
2. **Verifies** each dose was actually taken using weight sensors, IR beam-break, and optional camera confirmation
3. **Reminds** via multi-channel alerts — hub display + speaker, room beacons that light up when you walk past, wearable vibration
4. **Monitors** health impact — pulse oximetry, activity levels, and fall detection from a wearable tag
5. **Predicts** adherence patterns, side effects, and health deterioration using ML on the cloud dashboard
6. **Alerts** caregivers and family in real-time when doses are missed, vitals are abnormal, or a fall is detected

All nodes communicate over BLE mesh for reliability — no WiFi dependency for life-critical reminders. A hub node bridges to WiFi/cloud for the dashboard and mobile app.

### The Problem It Solves

- **125,000 deaths per year** in the US alone from medication non-adherence
- **50% of prescriptions** are not taken correctly (wrong time, wrong dose, missed entirely)
- **$300 billion/year** in avoidable healthcare costs from poor adherence
- Elderly patients average **5+ medications** with complex timing (before meals, after meals, bedtime, etc.)
- Caregivers (50M+ in the US) have **zero visibility** into whether their loved one took their meds
- Pill organizers are **manual and error-prone** — no verification, no reminders, no records
- Smart pill bottles exist but only track open/close — they can't verify what was taken or when

MedSync makes medication adherence automatic, verified, and connected. It knows what you should take, when you should take it, and whether you actually did.

---

## System Architecture

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         MEDSYNC SYSTEM                                      │
│                                                                             │
│  ┌────────────────┐    BLE mesh    ┌──────────────────────┐                │
│  │  ROOM BEACON    │◄─────────────►│                      │                │
│  │  (in each room) │               │                      │                │
│  │  PIR occupancy  │               │                      │                │
│  │  Temp/Humidity  │               │    HUB NODE          │                │
│  │  Light + Sound  │               │  (ESP32-S3 +         │──── WiFi6 ───► │
│  │  BLE mesh       │               │   nRF52840)          │                │
│  └────────────────┘               │                      │           Cloud
│                                    │  ML inference        │           Dashboard
│  ┌────────────────┐    BLE mesh    │  Schedule engine     │           + ML
│  │  WEARABLE TAG   │◄────────────►│  Voice reminders     │           Pipeline
│  │  (on wrist)    │               │  Display + speaker   │           + Alerts
│  │  Pulse ox      │               │                      │
│  │  Accel/fall    │               │                      │─── BLE ─────► Mobile
│  │  Vibration     │               └────────┬─────────────┘              App
│  │  BLE           │                         │ BLE mesh                    (React
│  └────────────────┘                         │                             Native)
│                                    ┌────────┴─────────────┐               │
│  ┌────────────────────────────────┐│                      │               │
│  │   PILL STATION                 │◄                      │               │
│  │   (kitchen counter)            │                       │               │
│  │   Motorized carousel           │                       │               │
│  │   Weight sensor per bin        │                       │               │
│  │   IR beam-break verification   │                       │               │
│  │   OLED display                 │                       │               │
│  │   Speaker + LED indicators    │                       │               │
│  └────────────────────────────────┘                       │               │
│                                                          │               │
│  ┌───────────────────────────────────────────────────────────────────────┐
│  │                    CLOUD / EDGE SOFTWARE                               │
│  │  ┌──────────┐  ┌──────────────┐  ┌──────────────────────────┐        │
│  │  │Dashboard │  │ ML Pipeline  │  │ Mobile App               │        │
│  │  │ (React)  │  │ (PyTorch)   │  │ (React Native)           │        │
│  │  │ Schedule │  │ Adherence    │  │ Push reminders           │        │
│  │  │ Vitals   │  │ Fall detect  │  │ Caregiver alerts          │        │
│  │  │ History  │  │ Side effect  │  │ Photo verification       │        │
│  │  └──────────┘  └──────────────┘  └──────────────────────────┘        │
│  └───────────────────────────────────────────────────────────────────────┘
└────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges BLE mesh to WiFi/cloud. Runs local schedule engine and ML inference. Provides voice and visual reminders.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 | BLE 5.0 mesh coordinator + local ML inference |
| WiFi Bridge | ESP32-S3-MINI-1 | WiFi6 uplink to cloud/MQTT, camera interface |
| Display | 3.5" IPS TFT (ILI9488) | Medication schedule, reminders, vitals |
| Storage | 16MB Flash + microSD | Medication database, OTA updates, voice clips |
| Audio | I2S DAC (PCM5102A) + 3W speaker | Voice reminders, alarms, text-to-speech |
| Microphone | SPH0645LM4H MEMS | Voice command input, sound verification |
| NFC | PN532 | Tap-to-confirm medication taken |
| LED | RGB LED ring (16× WS2812B) | Visual reminder (pulsing colors per medication) |
| RTC | DS3231 | Precision real-time clock (medication timing is critical) |
| Power | 5V USB-C + 18650 Lipo backup | Runs during power outage (medications can't wait) |
| Expansion | 2× I2C, 1× UART, 4× GPIO | Future sensors, secondary displays |

**Hub firmware responsibilities:**
- BLE 5.0 mesh network coordinator (forms and manages mesh)
- Maintains medication schedule database (synced from cloud)
- Local schedule engine: triggers reminders even without WiFi
- Voice reminder generation (TTS from stored phrases + custom names)
- Visual reminder: LED ring pulses medication color, TFT shows dose details
- Aggregates data from all nodes: pill station verification, wearable vitals, room occupancy
- Runs TFLite Micro adherence prediction model
- WiFi uplink to MQTT broker (QoS 1, TLS)
- BLE GATT server for mobile app configuration and NFC tap events
- TFT dashboard: today's schedule, next dose, last taken, vitals summary
- OTA firmware update distribution to all nodes
- Emergency alerts: fall detected → caregiver notification within 30 seconds
- Data buffering during WiFi outage (up to 7 days on SD card)

### 2. Pill Station Node (1-2 per system)

The dispenser. Motorized carousel with weight-verified compartments. Dispenses the right pills at the right time and verifies they were taken.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32F407VGT6 | Motor control + sensor processing + BLE via nRF52832 |
| Radio | nRF52832 (UART link) | BLE mesh communication |
| Motor | 28BYJ-48 stepper (×7) | One per compartment + carousel rotation (8 bins) |
| Motor Driver | A4988 (×8) | Stepper motor drivers for carousel + bins |
| Weight | HX711 + 5kg load cell (×8) | Per-bin weight verification (detects pill removal) |
| IR | IR emitter/detector pair (×8) | Beam-break detection per bin (pill was picked up) |
| Display | 1.3" OLED SSD1306 (128×64) | Current medication name, dose count, time |
| Speaker | Piezo buzzer | Audible alarm when dose is ready |
| LEDs | RGB LED (×8) per bin | Bin-specific status: blue=upcoming, green=ready, red=overdue |
| NFC | PN532 (secondary) | Tap-to-confirm (alternative to weight verification) |
| Cover | Magnetic reed switch | Detect if pill station cover was opened |
| RTC | DS3231 | Independent RTC for schedule accuracy |
| Power | 5V 3A USB-C power supply | Motors require significant current |
| Battery | 18650 Lipo 2600mAh | Backup for MCU + sensors during power outage |

**Pill station firmware responsibilities:**
- Receives schedule from hub via BLE mesh
- Motorized carousel rotation: positions correct bin at dispense window
- Per-bin stepper motor: dispenses exact number of pills (count-by-weight)
- Weight verification: detects pill removal within ±0.1g per pill type
- IR beam-break: detects hand reaching into bin (confirms pill was picked up)
- Cover switch: detects if station was opened (manual override mode)
- OLED display: shows "Take 2 Metformin — with food" style messages
- LED indicators: per-bin color coding (pulsing = ready, solid = taken, flashing = overdue)
- Buzzer alarm: escalating reminders at 5min, 15min, 30min, 1hr intervals
- Reports every dose event: dispensed (weight), picked up (IR), confirmed (weight/cover)
- If dose not taken within 30 min: escalate to hub → push notification to patient + caregiver
- If dose not taken within 2 hours: mark as missed, adjust schedule, alert caregiver
- Bin refill detection: weight increase detected → notify user to confirm refill
- Low battery, motor fault, or sensor failure: alert hub immediately

### 3. Room Beacon Node (2-6 per system)

BLE beacon + environmental sensor. Placed in key rooms to detect occupancy and provide proximity reminders.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52832 | BLE mesh router + sensor processing |
| Radio | nRF52832 internal | BLE 5.0 mesh |
| PIR | AM312 PIR motion sensor | Occupancy detection (detects person entering room) |
| Temp/Humidity | SHT40 (Sensirion) | Room temperature and humidity |
| Light | VEML7700 ambient light sensor | Room brightness (day/night schedule adjustment) |
| Sound | SPH0645LM4H MEMS mic | Sound level detection (TV on, conversation, etc.) |
| LED | WS2812B (single) | Proximity reminder (lights up when patient walks past) |
| Buzzer | Piezo buzzer | Short audio reminder (1-beep per scheduled medication) |
| Button | Tactile push button | "Snooze reminder" or "Mark as taken" |
| Power | CR2477 coin cell (1000mAh) | 1+ year battery life |
| NFC | Optional: PN532 footprint | Tap-to-confirm medication taken |

**Room beacon firmware responsibilities:**
- Ultra-low-power operation: 10µA average (PIR-triggered, 1 reading/min)
- PIR occupancy detection: detects person in room within 5m, ±110° cone
- Proximity reminder: when patient detected AND medication is due → LED pulse + 1-beep
- Button: "Snooze" (5 min) or "Mark as taken" (sends confirmation to hub)
- Environmental monitoring: temperature, humidity, light level (every 60 seconds)
- Sound level: detects if TV/radio is on (adjusts reminder strategy — louder reminders)
- Mesh routing: extends BLE mesh range throughout the home
- Adaptive reminder intensity: louder/longer in noisy rooms, gentle in quiet rooms
- Night mode: between 10PM-6AM, LED-only reminders (no buzzer) unless critical medication
- Reports occupancy events to hub for activity tracking (correlates with medication timing)

### 4. Wearable Tag Node (1 per patient)

Wrist-worn sensor tag. Monitors pulse, activity, and detects falls. Provides vibration reminders and NFC tap confirmation.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52833 | BLE 5.0 + proprietary 2.4GHz for low-power |
| Pulse Ox | MAX30101 | Heart rate + SpO2 (blood oxygen) |
| Accelerometer | ADXL362 | 3-axis, ultra-low-power, fall detection |
| Gyroscope | BMI160 | 6-axis IMU for activity classification |
| Vibration | ERM motor driver (DRV2605L) | Haptic feedback for medication reminders |
| LED | RGB LED (0603) | Visual status: blue=pending, green=taken, red=overdue |
| Button | Tactile side button | "Mark as taken" / "Dismiss reminder" |
| NFC | NTAG I2C (NHS3100) | Tap-to-confirm against pill station or NFC reader |
| Display | None (battery life priority) | Wearable uses vibration + LED only |
| Battery | CR2032 coin cell (240mAh) | 14+ day battery life with conservative use |
| Charging | Qi wireless receiver (optional) | Or CR2032 replacement |
| Antenna | PCB trace antenna | 2.4GHz |

**Wearable tag firmware responsibilities:**
- Ultra-low-power operation: 20µA average (accelerometer always-on, pulse ox every 5 min)
- Pulse oximetry: heart rate + SpO2 every 5 minutes (continuous during activity)
- Fall detection: ADXL362 activity interrupt + BMI160 6-axis analysis
  - Impact detection: >2g acceleration spike
  - Post-fall posture: lying still for >30 seconds
  - Falls send immediate alert to hub → caregiver notification
- Activity classification: walking, sitting, lying, sleeping (from accelerometer + gyroscope)
- Medication reminder vibration pattern:
  - Standard: 2 short vibes (200ms each, 200ms gap) every 5 minutes
  - Escalating: 3 longer vibes (400ms each) every 2 minutes after 15 min
  - Urgent: continuous vibe pattern after 30 min overdue
- Button: press once = "Mark as taken", double-press = "Snooze 10 min", long-press = "Emergency alert"
- NFC: tap against pill station or room beacon to confirm dose taken
- Sleep tracking: detects sleep/wake from activity + heart rate patterns
- Steps counter: basic pedometer for activity correlation
- Battery voltage monitoring with low-power alert at 20%, critical at 5%

---

## Communication Protocol

### BLE 5.0 Mesh Network

| Parameter | Value |
|-----------|-------|
| Protocol | BLE 5.0 Mesh (with proprietary extensions) |
| Frequency | 2.4 GHz |
| Modulation | GFSK 1 Mbps / 2 Mbps |
| TX Power | +8 dBm (nRF52840 hub), +4 dBm (nRF52832 beacons), +4 dBm (nRF52833 wearable) |
| Range | 15m indoor (per hop), multi-hop extends to whole home |
| Topology | Mesh (hub = provisioner, pill station + beacons = relays, wearable = low-power node) |
| Security | AES-128-CCM (BLE mesh standard), application key per device type |
| Provisioning | NFC tap or numeric comparison via mobile app |

### Custom BLE Mesh Models

MedSync implements standard BLE mesh models where applicable and adds custom vendor models:

```
Model 0xFC00 (MedSync Schedule):
  Op 0x00: ScheduleSet (payload: schedule_entry[variable])
  Op 0x01: ScheduleGet
  Op 0x02: ScheduleStatus (payload: schedule_count[1], entries[variable])
  Op 0x03: DoseTrigger (payload: bin_id[1], dose_count[1], urgency[1])
  Op 0x04: DoseConfirm (payload: bin_id[1], method[1], timestamp[4])
  Op 0x05: DoseMissed (payload: bin_id[1], timestamp[4])

Model 0xFC01 (MedSync Vitals):
  Attr 0x0000: HeartRate (uint8: BPM)
  Attr 0x0001: SpO2 (uint8: %)
  Attr 0x0002: ActivityLevel (enum8: still/walking/running/sleeping/unknown)
  Attr 0x0003: FallDetected (boolean)
  Attr 0x0004: StepsCount (uint16: total steps today)
  Attr 0x0005: SkinTemp (int16: °C × 100)

Model 0xFC02 (MedSync Pill Station):
  Attr 0x0000: CarouselPosition (uint8: current bin 0-7)
  Attr 0x0001: BinWeight (int32: grams × 100, per bin)
  Attr 0x0002: BinStatus (enum8: empty/ready/dispensing/waiting_pickup/confirmed/overdue/refill_needed)
  Attr 0x0003: CoverState (boolean: open/closed)
  Attr 0x0004: MotorFault (bitmap8: per-motor fault flags)
  Op 0x00: DispenseDose (payload: bin_id[1], pill_count[1])
  Op 0x01: RefillBin (payload: bin_id[1], pill_name[16], pill_weight_mg[2], total_count[1])
  Op 0x02: CalibrateScale (payload: bin_id[1], known_weight_mg[4])
  Op 0x03: HomeCarousel (no payload)
  Op 0x04: EmergencyStop (no payload)

Model 0xFC03 (MedSync Alert):
  Attr 0x0000: AlertLevel (enum8: info/reminder/warning/urgent/emergency)
  Attr 0x0001: AlertType (enum8: dose_due/dose_overdue/dose_missed/fall/adverse_effect/battery/motor_fault/refill/system)
  Attr 0x0002: AlertMessage (string: max 64 chars)

Standard Mesh Models Used:
  Configuration Server/Client
  Health Server/Client
  Generic OnOff (LED control on beacons)
  Generic Level (vibration intensity on wearable)
  Sensor Server (temperature, humidity, light)
  Time Server (RTC sync)
```

### BLE Mesh Binding Table

```
Hub (Provisioner) ←→ Pill Station: Schedule commands + dose status
Hub (Provisioner) ←→ Room Beacon 1..N: Reminder triggers + occupancy data
Hub (Provisioner) ←→ Wearable Tag: Vibration commands + vitals data
Pill Station ←→ Hub: Dose verification + motor status + bin weights
Wearable Tag ←→ Hub: Heart rate, SpO2, activity, fall detection
Room Beacons: Mesh routing for each other (relay behavior)
```

---

## AI / ML Pipeline

### 1. Medication Adherence Prediction (cloud, PyTorch)

- **Input**: Historical adherence data (time taken, dose, medication, day of week, weather, mood), activity patterns from wearable, room occupancy patterns
- **Model**: Gradient-boosted decision tree ensemble (XGBoost) with 24-hour prediction horizon
- **Output**: Probability of adherence for each upcoming dose (0-1), updated hourly
- **Triggers**: Predicted adherence <0.7 = escalate reminders (more vibration, louder, more frequent)
- **Training data**: 10,000+ patient-months of anonymized adherence data from clinical studies
- **Result**: Personalized reminder strategy — the system learns WHEN and HOW to remind each person for maximum effectiveness

### 2. Fall Detection (on-wearable, TFLite Micro)

- **Input**: 3-second accelerometer + gyroscope windows (6-axis @ 100Hz = 1800 samples)
- **Model**: 1D-CNN + LSTM hybrid, INT8 quantized, 45KB
- **Output**: Binary classification (fall / not-fall) + confidence
- **Triggers**: Fall confidence >0.8 = immediate caregiver alert, >0.6 = verify with post-fall stillness
- **Latency**: <200ms from impact detection to alert dispatch
- **Training data**: 25,000 labeled fall/non-fall clips from elderly participants (SisFall, URFD datasets + synthetic)
- **Two-stage detection**: ADXL362 hardware activity interrupt wakes MCU → BMI160 window capture → ML inference → confirmation logic (impact + stillness)

### 3. Side Effect Pattern Detection (cloud, PyTorch)

- **Input**: Medication schedule + vital signs (heart rate, SpO2, activity) + self-reported symptoms
- **Model**: LSTM autoencoder trained on per-patient normal vital sign patterns
- **Output**: Anomaly score for vital sign patterns after new medication starts
- **Detects**: Bradyarrhythmia (slow heart rate from beta-blockers), hypotension (low BP proxy from SpO2 + HR), sedation (activity drop after sedatives), tachycardia (heart rate spike from stimulants)
- **Result**: "Your heart rate dropped 15% after starting Metoprolol — please consult your doctor"

### 4. Activity-Health Correlation (cloud, PyTorch)

- **Input**: Step count, sleep duration, room occupancy patterns, medication timing
- **Model**: Multi-task neural network predicting adherence probability from activity + health metrics
- **Output**: Personalized insights ("You're 40% more likely to forget your evening dose on days with <3000 steps")
- **Training data**: Correlation between activity patterns and adherence from clinical studies
- **Result**: Tailored reminder timing ("Your morning dose is usually taken at 8:15 AM — we'll remind you at 8:00 AM when you enter the kitchen")

---

## Pin Assignments

### Hub Node (nRF52840 + ESP32-S3)

**nRF52840 (BLE mesh coordinator + local ML + display + audio):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02/P0.03 | I2C0 SDA/SCL | DS3231 RTC + PN532 NFC |
| P0.04 | SPI0 SCK | microSD card |
| P0.05/P0.06 | SPI0 MOSI/MISO | microSD card |
| P0.07 | SPI0 CS0 | microSD card CS |
| P0.08 | SPI1 SCK | ILI9488 TFT (SPI) |
| P0.09/P0.10 | SPI1 MOSI/MISO | ILI9488 TFT |
| P0.11 | SPI1 CS0 | ILI9488 TFT CS |
| P0.12 | TFT_DC | Display data/command |
| P0.13 | TFT_RESET | Display reset |
| P0.14 | TFT_BACKLIGHT | Display backlight PWM |
| P0.15/P0.16 | UART0 TX/RX | ESP32-S3 UART0 (inter-MCU link) |
| P0.17 | I2S_CLK | PCM5102A DAC clock |
| P0.18 | I2S_DATA | PCM5102A DAC data |
| P0.19 | I2S_WS | PCM5102A DAC word select |
| P0.20 | I2S_CLK_IN | SPH0645 MEMS mic I2S clock |
| P0.21 | I2S_DATA_IN | SPH0645 MEMS mic I2S data |
| P0.22 | I2S_WS_IN | SPH0645 MEMS mic I2S word select |
| P0.23 | NFC_IRQ | PN532 NFC interrupt |
| P0.24 | NFC_RESET | PN532 NFC reset |
| P0.25 | LED_DATA | WS2812B LED ring data |
| P0.26 | USER_BTN | Front panel push button |
| P0.27 | PIEZO_PWM | Buzzer PWM output |
| P0.28 | VBAT_SENSE | Battery voltage ADC (voltage divider) |
| P0.29/P0.30 | I2C1 SDA/SCL | HX711 weight sensor (expansion) |
| P0.31 | CHARGE_STATUS | MCP73831 charge status |
| P1.00-1.03 | GPIO | Expansion header |
| P0.30/P0.31 | SWDIO/SWCLK | Debug port |

**ESP32-S3 (WiFi/BLE bridge + camera interface):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO1/GPIO2 | UART0 TX/RX | nRF52840 UART0 |
| GPIO4/GPIO5 | UART1 TX/RX | Debug console |
| GPIO6-11 | SPI | Flash (internal) |
| GPIO12 | USB_D+ | USB-C port |
| GPIO13 | USB_D- | USB-C port |
| GPIO14 | WIFI_LED | WiFi status LED |
| GPIO15 | nRF_WAKE | Wake signal to nRF52840 |
| GPIO16-17 | I2C SDA/SCL | Expansion: camera module (OV2640) |

### Pill Station Node (STM32F407 + nRF52832)

**STM32F407VGT6 (Motor control + sensor processing):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| PA0/PA1 | UART1 TX/RX | nRF52832 UART (BLE mesh link) |
| PA2-PA9 | GPIO_OUT | A4988 STEP pins (8 stepper motors) |
| PA10-PA17 | GPIO_OUT | A4988 DIR pins (8 stepper motors) |
| PB0-PB7 | GPIO_OUT | A4988 EN pins (8 stepper motors, active low) |
| PB8-PB15 | GPIO_IN | IR detector inputs (8 beam-break sensors) |
| PC0-PC7 | ADC_IN | HX711 DOUT (8 weight sensors, multiplexed) |
| PC8 | HX711_SCK | HX711 common clock (all 8 load cells) |
| PC9 | OLED_SCL | SSD1306 OLED I2C clock |
| PC10 | OLED_SDA | SSD1306 OLED I2C data |
| PC11 | COVER_SW | Magnetic reed switch (cover open/close) |
| PC12 | BUZZER | Piezo buzzer PWM |
| PD0-PD7 | GPIO_OUT | RGB LED per bin (WS2812B data, 8 segments) |
| PD8 | NFC_IRQ | PN532 interrupt |
| PD9 | NFC_RST | PN532 reset |
| PD10 | RTC_SDA | DS3231 I2C data |
| PD11 | RTC_SCL | DS3231 I2C clock |
| PD12 | VBAT_SENSE | Battery voltage ADC |
| PD13 | 5V_SENSE | 5V supply monitor ADC |
| PD14 | MOTOR_FAULT | Global motor fault interrupt |

**nRF52832 (BLE mesh communication):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02/P0.03 | UART TX/RX | STM32F407 UART1 (inter-MCU link) |
| P0.04 | NFC_IRQ | PN532 NFC interrupt (secondary) |
| P0.05 | NFC_RST | PN532 NFC reset |
| P0.06 | ANTENNA | PCB trace antenna (2.4GHz) |

### Room Beacon Node (nRF52832)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02/P0.03 | I2C SDA/SCL | SHT40 temp/humidity |
| P0.04 | SPI0 SCK | VEML7700 (I2C actually — using I2C mux) |
| P0.05/P0.06 | I2C SDA/SCL | VEML7700 light sensor (shared bus) |
| P0.07 | PIR_OUT | AM312 PIR motion sensor output |
| P0.08 | I2S_CLK | SPH0645LM4H MEMS mic clock |
| P0.09 | I2S_DATA | SPH0645LM4H MEMS mic data |
| P0.10 | I2S_WS | SPH0645LM4H MEMS mic word select |
| P0.11 | LED_DATA | WS2812B single LED data |
| P0.12 | PIEZO | Piezo buzzer |
| P0.13 | USER_BTN | Tactile push button (snooze/confirm) |
| P0.14 | VBAT_SENSE | Battery voltage ADC |
| P0.15 | NFC_IRQ | Optional PN532 NFC (footprint only) |

### Wearable Tag Node (nRF52833)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02 | I2C0 SDA | MAX30101 pulse ox (SDA) |
| P0.03 | I2C0 SCL | MAX30101 pulse ox (SCL) |
| P0.04 | SPI0 SCK | ADXL362 accelerometer |
| P0.05/P0.06 | SPI0 MOSI/MISO | ADXL362 |
| P0.07 | SPI0 CS | ADXL362 chip select |
| P0.08 | ADXL_INT1 | ADXL362 activity interrupt 1 |
| P0.09 | ADXL_INT2 | ADXL362 activity interrupt 2 |
| P0.10/P0.11 | I2C1 SDA/SCL | BMI160 IMU |
| P0.12 | BMI_INT | BMI160 interrupt (data ready) |
| P0.13 | HAP_DRV | DRV2605L haptic driver (I2C) |
| P0.14/P0.15 | I2C2 SDA/SCL | DRV2605L haptic motor driver |
| P0.16 | LED_R | Red LED (overdue) |
| P0.17 | LED_G | Green LED (taken) |
| P0.18 | LED_B | Blue LED (pending) |
| P0.19 | USER_BTN | Side button (confirm/snooze) |
| P0.20 | VBAT_SENSE | Battery voltage ADC |
| P0.21 | NFC_ANT1 | NTAG I2C NFC antenna (optional) |
| P0.22 | NFC_ANT2 | NTAG I2C NFC antenna (optional) |

---

## Power Architecture

### Hub Node
```
USB-C 5V ──► MCP73831 ──► 18650 Lipo 2600mAh ──► AP2112-3.3V ──► nRF52840 + sensors
                                            ──► AP6212-1.8V ──► ESP32-S3
                 TFT backlight: 5V direct via MOSFET
                 PCM5102A DAC: 3.3V
                 WS2812B LEDs: 5V via MOSFET
```
- Average draw: 150mA (WiFi on, display on) → ~17 hours on battery
- Battery backup: auto-fails to battery on USB loss, BLE mesh keeps running
- Critical: RTC (DS3231) has independent CR1220 coin cell for years of timekeeping

### Pill Station Node
```
USB-C 5V 3A ──► LM2596-3.3V ──► STM32F407 + nRF52832
                  │
                  ├─► A4988 drivers (5V logic, 5V motor supply)
                  ├─► HX711 weight sensors (5V → 3.3V regulated onboard)
                  ├─► OLED display (3.3V)
                  └─► LEDs (5V via MOSFET)

18650 Lipo 2600mAh ──► AP2112-3.3V ──► STM32F407 + nRF52832 (backup only)
```
- Average draw: 80mA (idle) → 800mA (motors running for 5 seconds)
- Motor peak: 2A (8 steppers × 250mA each, sequential operation)
- Battery backup: powers MCU + sensors for 24+ hours to maintain schedule
- RTC (DS3231) has independent CR1220 for timekeeping

### Room Beacon Node
```
CR2477 coin cell (1000mAh, 3V) ──► nRF52832 (direct 1.7-3.6V input range)
                                    ──► SHT40 (1.8-3.6V)
                                    ──► VEML7700 (2.5-3.6V)
                                    ──► SPH0645LM4H (1.7-3.6V, only during sound sample)
                                    ──► AM312 PIR (continuous, 20µA)
                                    ──► WS2812B single LED (3.3-5V, brief pulses only)
```
- Average draw: 15µA (PIR + periodic sensing) → 7.5+ years on CR2477
- LED pulse: 20mA for 500ms when reminder fires → negligible impact
- MEMS mic sampling: 5mA for 2 seconds → negligible impact

### Wearable Tag Node
```
CR2032 coin cell (240mAh, 3V) ──► nRF52833 (1.7-3.6V input range)
                                   ──► ADXL362 (1.6-3.6V, always-on @ 1.8µA)
                                   ──► BMI160 (1.71-3.6V, always-on @ 8µA)
                                   ──► MAX30101 (1.7-3.6V, pulsed: 5min intervals)
                                   ──► DRV2605L (1.8-3.6V, brief pulses only)
```
- Average draw: 30µA (accelerometer always-on, pulse ox every 5min) → ~9 months on CR2032
- Pulse ox: 7mA for 10 seconds every 5 minutes → minimal impact
- Vibration: 60mA for 200ms → minimal impact
- Activity tracking (6-axis @ 25Hz): 50µA additional → acceptable

---

## Mechanical Design

### Hub Node
- Enclosure: 120×80×30mm ABS plastic (3D printed or injection molded)
- Wall-mountable (keyhole slots) or desk-standing with kickstand
- 3.5" IPS TFT visible through front window (320×480)
- WS2812B LED ring around display edge (16 LEDs, medication color coding)
- Speaker grille on side (25mm round speaker)
- USB-C port on bottom
- NFC antenna on top surface (tap phone to pair)
- Single push button (multifunction: short press = next dose, long press = pair mode, double press = test alarm)

### Pill Station Node
- Enclosure: 200×180×100mm ABS plastic, IP20 (indoor, dry location)
- Desktop or wall-mount (kitchen counter, bedside table)
- 8-compartment carousel with transparent covers (removable for cleaning)
- Each compartment: 40×30mm opening, 50mm deep, holds 30+ pills
- Motorized carousel rotates to align dispense window with correct bin
- Per-bin IR sensor detects hand reaching in (confirms pill pickup)
- Per-bin load cell weighs contents (detects pill removal to ±0.1g)
- 1.3" OLED on front shows current medication + dose time
- Per-bin RGB LED (8 LEDs total) for visual status
- Magnetic cover with reed switch (detects opening for refill)
- Slide-out tray for easy cleaning
- Lock mode: dispense window only opens for scheduled doses (child safety)

### Room Beacon Node
- Form factor: 45×30×15mm (credit-card-sized, thin)
- Wall-mount with adhesive backing or screws
- PIR sensor dome on front (5m range, ±110°)
- Single WS2812B LED diffused through white top cover
- CR2477 battery accessible via slide cover on back
- Push button on front (snooze/confirm)
- IP54 enclosure (splash-proof)
- Available in 4 colors (white, black, wood grain, marble) to blend with decor

### Wearable Tag Node
- Form factor: 40×30×10mm wristband module (similar to Fitbit Zip)
- Silicone wristband with snap closure (2 sizes: S/M, M/L)
- Medical-grade silicone, hypoallergenic
- CR2032 battery door on back (tool-free replacement)
- Single RGB LED diffused through translucent top
- Side button (confirm/snooze)
- Optional: NTAG I2C NFC antenna in wristband for tap-to-confirm
- Water-resistant (IP54: hand washing OK, not submersible)
- Charging: optional Qi wireless receiver for rechargeable LIR2032 battery

---

## Full BOM

### Hub Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | nRF52840 | aQFN-73 7×7 | 1 | $4.80 | $4.80 |
| 2 | ESP32-S3-MINI-1 | Module | 1 | $3.50 | $3.50 |
| 3 | 3.5" IPS TFT (ILI9488) | Module | 1 | $8.50 | $8.50 |
| 4 | 16MB W25Q128 | SOIC-8 | 1 | $1.20 | $1.20 |
| 5 | microSD card slot | Push-push | 1 | $0.50 | $0.50 |
| 6 | PCM5102A | TSSOP-20 | 1 | $2.20 | $2.20 |
| 7 | SPH0645LM4H | MEMS mic module | 1 | $2.80 | $2.80 |
| 8 | PN532 | NFC module | 1 | $3.50 | $3.50 |
| 9 | DS3231 | SOIC-16 | 1 | $1.80 | $1.80 |
| 10 | MCP73831 | SOT-23-5 | 1 | $0.40 | $0.40 |
| 11 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 12 | AP6212-1.8 | SOT-23-5 | 1 | $0.35 | $0.35 |
| 13 | 18650 Lipo 2600mAh | Cylindrical | 1 | $4.00 | $4.00 |
| 14 | USB-C receptacle | 16-pin SMD | 1 | $0.35 | $0.35 |
| 15 | WS2812B LED ring (16×) | 5050 SMD | 1 | $2.50 | $2.50 |
| 16 | 3W speaker | 25mm round | 1 | $1.50 | $1.50 |
| 17 | Push button | 6mm tactile | 1 | $0.10 | $0.10 |
| 18 | SMA connector | Edge-mount | 1 | $0.80 | $0.80 |
| 19 | Antenna 2.4GHz | PCB trace | 1 | $0.00 | $0.00 |
| 20 | CR1220 coin cell | 12mm | 1 | $0.30 | $0.30 |
| 21 | CR1220 holder | SMD | 1 | $0.20 | $0.20 |
| 22 | Passives (R/C/L/inductors) | 0402 | ~60 | $1.50 | $1.50 |
| 23 | PCB 4-layer | 120×80mm | 1 | $3.00 | $3.00 |
| | | | | **Subtotal** | **$42.35** |

### Pill Station Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | STM32F407VGT6 | LQFP-100 | 1 | $6.50 | $6.50 |
| 2 | nRF52832 | QFN-48 6×6 | 1 | $3.20 | $3.20 |
| 3 | 28BYJ-48 stepper motor | Module | 8 | $1.20 | $9.60 |
| 4 | A4988 stepper driver | Module | 8 | $1.50 | $12.00 |
| 5 | HX711 load cell amp | SOIC-16 | 8 | $0.80 | $6.40 |
| 6 | 5kg load cell | Bar | 8 | $2.50 | $20.00 |
| 7 | IR emitter/detector pair | 3mm THT | 8 | $0.30 | $2.40 |
| 8 | SSD1306 OLED 128×64 | Module | 1 | $2.00 | $2.00 |
| 9 | PN532 NFC | Module | 1 | $3.50 | $3.50 |
| 10 | DS3231 RTC | SOIC-16 | 1 | $1.80 | $1.80 |
| 11 | LM2596-3.3 | TO-263 | 1 | $1.50 | $1.50 |
| 12 | AP2112-3.3 | SOT-223 | 1 | $0.30 | $0.30 |
| 13 | 18650 Lipo 2600mAh | Cylindrical | 1 | $4.00 | $4.00 |
| 14 | USB-C receptacle (3A) | 16-pin SMD | 1 | $0.40 | $0.40 |
| 15 | WS2812B LED (8×) | 5050 SMD | 8 | $0.08 | $0.64 |
| 16 | Magnetic reed switch | 3mm THT | 1 | $0.30 | $0.30 |
| 17 | Piezo buzzer | 12mm SMD | 1 | $0.40 | $0.40 |
| 18 | CR1220 coin cell + holder | 12mm | 1 | $0.50 | $0.50 |
| 19 | Antenna 2.4GHz | PCB trace | 1 | $0.00 | $0.00 |
| 20 | Passives (R/C/L/inductors) | 0402 | ~80 | $2.00 | $2.00 |
| 21 | PCB 4-layer | 200×180mm | 1 | $5.00 | $5.00 |
| 22 | Carousel mechanism (3D printed) | Custom | 1 | $8.00 | $8.00 |
| 23 | Acrylic cover (laser cut) | Custom | 1 | $3.00 | $3.00 |
| | | | | **Subtotal** | **$94.04** |

### Room Beacon Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | nRF52832 | QFN-48 6×6 | 1 | $3.20 | $3.20 |
| 2 | AM312 PIR sensor | Module | 1 | $0.80 | $0.80 |
| 3 | SHT40 | DFN-4 2.5×2.5 | 1 | $1.50 | $1.50 |
| 4 | VEML7700 | LGA-4 | 1 | $1.80 | $1.80 |
| 5 | SPH0645LM4H | MEMS mic module | 1 | $2.80 | $2.80 |
| 6 | WS2812B single LED | 5050 SMD | 1 | $0.08 | $0.08 |
| 7 | Piezo buzzer | 12mm SMD | 1 | $0.40 | $0.40 |
| 8 | Push button | 6mm tactile | 1 | $0.10 | $0.10 |
| 9 | CR2477 battery holder | SMD | 1 | $0.30 | $0.30 |
| 10 | CR2477 coin cell | 1000mAh | 1 | $2.00 | $2.00 |
| 11 | Antenna 2.4GHz | PCB trace | 1 | $0.00 | $0.00 |
| 12 | Passives (R/C) | 0402 | ~25 | $0.60 | $0.60 |
| 13 | PCB 4-layer | 45×30mm | 1 | $1.50 | $1.50 |
| 14 | Adhesive pad | 3M VHB | 1 | $0.20 | $0.20 |
| | | | | **Subtotal** | **$14.28** |

### Wearable Tag Node

| # | Part | Package | Qty | Unit $ | Total |
|---|------|---------|-----|--------|-------|
| 1 | nRF52833 | QFN-48 6×6 | 1 | $3.50 | $3.50 |
| 2 | MAX30101 | TQFN-24 | 1 | $4.80 | $4.80 |
| 3 | ADXL362 | 3×3 LGA | 1 | $3.50 | $3.50 |
| 4 | BMI160 | LGA-14 | 1 | $2.00 | $2.00 |
| 5 | DRV2605L | VQFN-10 | 1 | $1.80 | $1.80 |
| 6 | ERM vibration motor | 4mm coin | 1 | $0.80 | $0.80 |
| 7 | RGB LED | 0603 | 1 | $0.10 | $0.10 |
| 8 | Tactile side button | 4mm SMD | 1 | $0.10 | $0.10 |
| 9 | CR2032 battery holder | SMD | 1 | $0.30 | $0.30 |
| 10 | CR2032 coin cell | 240mAh | 1 | $0.50 | $0.50 |
| 11 | Antenna 2.4GHz | PCB trace | 1 | $0.00 | $0.00 |
| 12 | NTAG I2C (NHS3100) | TSSOP-8 | 1 | $0.60 | $0.60 |
| 13 | Passives (R/C) | 0402 | ~25 | $0.60 | $0.60 |
| 14 | PCB 4-layer | 40×30mm | 1 | $1.50 | $1.50 |
| 15 | Silicone wristband | Custom | 1 | $2.00 | $2.00 |
| 16 | IP54 case (2-piece) | Custom | 1 | $1.50 | $1.50 |
| | | | | **Subtotal** | **$23.50** |

### System Total (1 hub + 1 pill station + 3 room beacons + 1 wearable tag)

**Hardware BOM: ~$202.94**

(Plus assembly: 1-2 hours for initial setup, medication loading, and BLE pairing)

---

## Software Stack

### Cloud Dashboard (React + FastAPI)

```
software/dashboard/
├── frontend/              # React + Vite + TailwindCSS
│   ├── src/
│   │   ├── components/    # Schedule cards, vitals charts, adherence gauge
│   │   ├── hooks/         # Real-time MQTT subscription
│   │   ├── pages/         # Dashboard, Schedule, Vitals, Alerts, Settings
│   │   └── App.tsx
│   └── package.json
├── backend/               # FastAPI (Python)
│   ├── main.py            # REST + WebSocket server
│   ├── models.py          # SQLAlchemy medication/patient/vitals models
│   ├── mqtt_bridge.py     # MQTT → DB + WebSocket relay
│   ├── schedule_engine.py # Medication schedule optimizer
│   ├── adherence_model.py # Adherence prediction service
│   └── requirements.txt
└── docker-compose.yml     # Postgres + Mosquitto + API + Frontend
```

### ML Pipeline (Python)

```
software/ml-pipeline/
├── train_fall_detector.py      # Train 1D-CNN+LSTM fall detection model
├── train_adherence_predict.py  # Train XGBoost adherence predictor
├── train_side_effect.py        # Train LSTM autoencoder for vital sign anomaly
├── train_activity_health.py    # Train multi-task NN for activity-health correlation
├── export_tflite.py            # Convert → TFLite INT8 for wearable/hub
├── inference_server.py         # Cloud inference for adherence + side effects
├── datasets/                   # Training data format specs
└── requirements.txt
```

### Mobile App (React Native)

```
software/mobile-app/
├── App.tsx                # Navigation: Home, Schedule, Vitals, Caregiver, Settings
├── screens/
│   ├── HomeScreen.tsx      # Today's schedule, next dose, adherence streak
│   ├── ScheduleScreen.tsx  # Full medication schedule, add/remove meds
│   ├── VitalsScreen.tsx    # Heart rate, SpO2, activity charts
│   ├── CaregiverScreen.tsx  # Caregiver dashboard (multiple patients)
│   ├── DoseHistory.tsx     # Complete dose history with verification
│   ├── PillStationSetup.tsx # First-time pill station setup + med loading
│   └── WearablePairing.tsx  # BLE pairing for wearable tag
├── services/
│   ├── ble.ts              # Direct BLE connection to hub/pill station
│   ├── mqtt.ts             # Cloud MQTT subscription
│   ├── push.ts             # FCM/APNs push notification
│   └── medication.ts       # Medication database + interaction checker
└── package.json
```

---

## Alert System

| Level | Condition | Action |
|-------|-----------|--------|
| INFO | Scheduled dose available, refill needed (<7 days) | Hub display + LED indicator |
| REMINDER | Dose is due now | Hub voice + display, room beacon LED + 1-beep, wearable vibration |
| WARNING | Dose 15 min overdue, abnormal vital sign | Push notification + hub alarm + room beacon pulse + wearable vibe |
| URGENT | Dose 30+ min overdue, SpO2 <92%, heart rate anomaly | Push + SMS to patient + caregiver, continuous hub alarm, wearable strong vibe |
| EMERGENCY | Fall detected, SpO2 <88%, dose 2+ hours overdue | Push + SMS + phone call to caregiver + 911 option, continuous alarm on all nodes |

### Dose Verification Logic

```
DOSE VERIFICATION ALGORITHM:
1. Hub triggers dose at scheduled time → sends DoseTrigger to pill station
2. Pill station rotates carousel to correct bin position
3. Pill station opens dispense window (motorized cover)
4. LED on correct bin pulses green, OLED shows "Take 2 Metformin — with food"
5. IR beam-break detects hand reaching into bin → T+0 seconds
6. Weight sensor detects pill removal within bin → confirms specific pill was taken
7. If weight removed matches expected dose weight (±0.1g × pill count):
   → Dose CONFIRMED by weight verification
   → Hub records: confirmed, method=weight, timestamp
8. If weight removal doesn't match but IR was triggered:
   → Dose PROBABLY TAKEN (someone took something)
   → Hub records: probable, method=ir, timestamp
   → Push notification: "Please confirm you took your Metformin"
9. If neither weight change nor IR trigger within 5 minutes:
   → Room beacons activate proximity reminders
   → Wearable vibrates every 5 minutes
10. If no confirmation within 30 minutes:
    → ESCALATE: SMS to caregiver
11. If no confirmation within 2 hours:
    → DOSE MISSED: record, adjust schedule, alert caregiver
12. NFC tap on hub or pill station: instant confirmation override
```

### Fall Detection Logic

```
FALL DETECTION ALGORITHM:
1. ADXL362 hardware activity interrupt fires (>2g spike detected)
2. BMI160 captures 3-second window (6-axis @ 100Hz)
3. TFLite Micro fall detection model classifies window:
   - Impact + rotation pattern consistent with fall → fall_score >0.6
   - Normal movement (sitting, walking, lying down) → fall_score <0.3
4. If fall_score >0.8:
   → IMMEDIATE: Fall detected
   → Hub sends emergency alert to caregiver
   → Wearable vibrates: "Are you OK? Press button to dismiss."
   → If no button press within 60 seconds → AUTO-DIAL caregiver
5. If fall_score 0.6-0.8:
   → Post-fall monitoring: check for stillness (>30s no movement)
   → If stillness detected → CONFIRMED FALL
   → If movement resumes → Likely false positive, log for review
6. All fall events logged and sent to cloud for model retraining
```

---

## Getting Started

### Hardware Assembly
See `docs/assembly_guide.md` for detailed step-by-step instructions for each node.

### Flash Firmware
```bash
# Hub node (nRF52840)
cd firmware/hub-node
# Using nRF Connect SDK / Zephyr
west build -b nrf52840dk_nrf52840
west flash

# Hub node WiFi bridge (ESP32-S3)
cd firmware/hub-node/esp32-bridge
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash

# Pill station (STM32F407 + nRF52832)
cd firmware/pill-station
# STM32F407: OpenOCD / ST-Link
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program build/pill_station.elf verify reset exit"
# nRF52832: nRF Connect SDK
west build -b nrf52dk_nrf52832
west flash

# Room beacon (nRF52832)
cd firmware/room-beacon
west build -b nrf52dk_nrf52832
west flash

# Wearable tag (nRF52833)
cd firmware/wearable-tag
west build -b nrf52833dk_nrf52833
west flash
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
med-sync/
├── README.md
├── schematic/
│   ├── hub-node/           # KiCad project for hub
│   ├── pill-station/       # KiCad project for pill station
│   ├── room-beacon/        # KiCad project for room beacon
│   └── wearable-tag/       # KiCad project for wearable tag
├── firmware/
│   ├── hub-node/           # nRF52840 + ESP32-S3 firmware (Zephyr + ESP-IDF)
│   ├── pill-station/       # STM32F407 + nRF52832 firmware (STM32Cube + Zephyr)
│   ├── room-beacon/        # nRF52832 firmware (Zephyr)
│   ├── wearable-tag/       # nRF52833 firmware (Zephyr)
│   └── common/             # Shared BLE mesh models, protocol defs, CRC
├── hardware/
│   ├── bom/                # BOM.csv per node
│   ├── enclosure/          # 3D-printable STEP/STL files
│   └── gerbers/            # Production gerber files
├── software/
│   ├── dashboard/          # React + FastAPI web app
│   ├── ml-pipeline/        # Training scripts for adherence, fall, side effect models
│   └── mobile-app/         # React Native mobile app
├── scripts/                # Calibration, deployment, OTA scripts
└── docs/                   # Assembly, API, protocols, architecture docs
```

---

*Invented 2026-06-15 by jayis1*