# SleepSync

**Intelligent sleep environment optimization system — monitors your sleep, controls your bedroom, and uses AI to give you the best sleep of your life.**

---

## What It Does

SleepSync is a 4-node system that turns your bedroom into an AI-optimized sleep sanctuary:

1. **Tracks** your sleep stages (wake, light, deep, REM) in real-time using ballistocardiography + actigraphy from an ultra-thin under-pillow strip
2. **Controls** room climate (temperature, humidity, CO2) by interfacing with your HVAC, humidifier, or portable heater/cooler
3. **Manages** light — motorizes your window shades, runs dawn-simulating wake-up lighting, enforces darkness for sleep onset
4. **Generates** adaptive soundscapes (white noise, pink noise, nature sounds) that respond to your sleep state and mask disruptions
5. **Learns** what environmental conditions give you the best sleep over time — and automatically replicates them
6. **Alerts** you to sleep disorders: detects chronic snoring, apnea patterns, restless legs, and circadian misalignment

All nodes communicate over BLE 5 mesh (short range, low power, perfect for a single room). The nightstand hub bridges to WiFi/cloud for the dashboard, ML pipeline, and mobile app.

### The Problem It Solves

- **1 in 3 adults** don't get enough sleep — the CDC calls insufficient sleep a public health epidemic
- Poor sleep costs the US economy **$411 billion/year** in lost productivity
- Sleep environment (temp, humidity, CO2, noise, light) accounts for **30-40% of sleep quality variance**
- Most people have no idea what their optimal sleep temperature is (studies show 65-68°F / 18-20°C, but it's individual)
- 22 million Americans have sleep apnea — **80% are undiagnosed**
- Seasonal affective disorder and circadian rhythm disruption affect **10 million+** Americans
- Current sleep trackers only measure — they don't **act**. SleepSync measures AND optimizes.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        SLEEPSYNC SYSTEM                                   │
│                                                                           │
│  ┌──────────────────┐   BLE 5    ┌──────────────────┐                   │
│  │  SLEEP STRIP      │◄─────────►│                   │                   │
│  │  (under-pillow)  │   mesh    │                   │                   │
│  │  BCG + actigraphy│          │  NIGHTSTAND HUB   │                   │
│  │  Heart + breath  │          │  (ESP32-S3)        │                   │
│  │  Snoring detect  │          │  Audio engine      │──── WiFi ────► Cloud
│  └──────────────────┘          │  Env sensors       │                  Dashboard
│                                │  TFT display        │                  + ML Pipeline
│  ┌──────────────────┐          │  BLE mesh provisioner│                 + Sleep Coach
│  │  CLIMATE NODE     │◄───────►│                   │                    + Alerts
│  │  (wall-mounted)  │   BLE 5 │                   │                   │
│  │  HVAC interface  │   mesh  │                   │─── BLE ──────► Mobile App
│  │  Humidity control│          │                   │                  (React Native)
│  │  Temp + RH sensor│          │                   │                   │
│  └──────────────────┘          └──────┬───────────┘                   │
│  ┌──────────────────┐                 │ BLE mesh                        │
│  │  SHADE CONTROLLER │◄───────────────┘                                │
│  │  (window frame)  │  (up to 4 shade controllers                      │
│  │  Motorized shade │   per hub, one per window)                       │
│  │  Dawn simulator  │                                                 │
│  │  Light sensor     │                                                 │
│  └──────────────────┘                                                  │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                    CLOUD / EDGE SOFTWARE                          │  │
│  │  ┌──────────┐  ┌──────────────┐  ┌──────────────────────────┐  │  │
│  │  │Dashboard │  │ ML Pipeline  │  │ Mobile App               │  │  │
│  │  │ (React) │  │ (TF/PyTorch) │  │ (React Native)           │  │  │
│  │  │ Sleep    │  │ Sleep staging│  │ Sleep score              │  │  │
│  │  │ History  │  │ Env optimize │  │ Smart alarm              │  │  │
│  │  │ Env data │  │ Apnea detect │  │ Environment controls     │  │  │
│  │  └──────────┘  └──────────────┘  └──────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Nightstand Hub (1 per system)

The brain. Bridges BLE mesh to WiFi/cloud. Runs the audio engine and on-device ML inference.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3-WROOM-1-N8R8 | Dual-core 240MHz, WiFi6 + BLE5, 8MB PSRAM, runs audio + TFLite |
| Display | 3.5" IPS TFT (ILI9488) | 480×320, shows sleep stage, score, environment, clock |
| Audio DAC | UDA1334A (I2S) | Hi-fi audio output for soundscapes |
| Speaker | 3W 40mm full-range | White noise, nature sounds, alarm |
| Mic | SPH0645LM4H (I2S) | Room noise monitoring, snoring detection |
| Temp/Humidity | BME280 | ±0.5°C, ±2%RH |
| CO2 | SCD40 (photoacoustic) | ±40ppm CO2 |
| Ambient Light | TSL2591 | 188µlux – 88,000lux |
| Storage | 16MB Flash + microSD | Audio files, sleep data log, OTA |
| Power | 5V USB-C + 2000mAh LiPo | Stays running during power outage |
| Buttons | 3× tactile | Snooze, mode, brightness |
| LED | RGB WS2812B | Clock/sleep stage indicator |

**Nightstand hub firmware responsibilities:**
- BLE 5 mesh provisioner (configures and manages all nodes)
- Real-time sleep staging from sleep strip data (TFLite Micro on ESP32-S3)
- Audio engine: adaptive soundscape generation (white/pink/brown noise, rain, ocean, forest)
- Soundscape adaptation: adjusts volume/timbre based on sleep stage + room noise
- TFT display: sleep stage ring, environment bars, clock, morning briefing
- WiFi uplink to MQTT broker (QoS 1, TLS 1.3)
- BLE GATT server for mobile app direct connection
- Smart alarm: wakes you at the optimal point in your lightest sleep within a 30-min window
- Local data logging to SD card (7-day buffer)
- OTA update distribution to all nodes

### 2. Sleep Strip (1 per bed)

Ultra-thin flexible sensor strip placed under your pillow. Measures your body's micro-movements through ballistocardiography (BCG) — every heartbeat, every breath, every toss and turn.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52832-QFAA | BLE 5, 64MHz Cortex-M4, ultra-low-power (5.2µA sleep) |
| Accelerometer | LIS3DH (3-axis, 16-bit) | Actigraphy — gross body movement, sleep/wake |
| Pressure Array | 8× FSR-406 (force-sensing resistors) | BCG — heart rate, breathing rate, snoring |
| Signal Conditioning | 2× HX711 (24-bit ADC) | High-resolution pressure signal acquisition |
| Battery | 100mAh LiPo (flex PCB) | 14-day runtime on single charge |
| Charging | Qi wireless receiver (MP2692A) | Drop on nightstand Qi pad to charge |
| Enclosure | Medical-grade silicone sleeve, 180×40×4mm | Waterproof, hypoallergenic, flexible |
| BLE Antenna | On-board PCB trace | 2.4GHz, ~10m range (bed to nightstand) |

**Sleep strip firmware responsibilities:**
- Samples BCG signal at 200Hz from FSR array via HX711
- Samples 3-axis acceleration at 50Hz from LIS3DH
- On-board preprocessing: FIR bandpass filter (0.5-40Hz for cardiac, 0.1-0.5Hz for respiration)
- Extracts heart rate, breathing rate, movement intensity every 5 seconds
- Detects snoring events (>40Hz pressure oscillation in BCG)
- BLE transmission to hub every 5 seconds (sleep features packet)
- Adaptive sampling: reduces rate during confirmed deep sleep (saves battery)
- Self-test: detects sensor disconnection, low battery, motion artifacts

### 3. Climate Node (1 per bedroom)

Wall-mounted climate controller. Interfaces with your existing HVAC, portable heater, humidifier, or dehumidifier to maintain the optimal sleep environment.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-C3-MINI-1 | BLE 5 + WiFi, RISC-V 160MHz, low cost |
| Temp/Humidity | SHT40 (Sensirion) | ±0.2°C, ±1.8%RH (high accuracy) |
| IR Blaster | IR LED (TSAL6100) + driver | Control existing AC/heater via infrared |
| Relay 1 | 5V relay (SRD-05VDC-SL-C) | Switch portable heater/fan on/off |
| Relay 2 | 5V relay (SRD-05VDC-SL-C) | Switch humidifier/dehumidifier on/off |
| Triac | BTA16-600BW | Dimmable control for resistive heater element |
| Display | 1.3" OLED (SH1106) | Current temp/RH, setpoint, status |
| Power | 5V USB-C (wall adapter) | Always powered (wall-mounted) |
| Enclosure | 85×85×25mm ABS, wall-mount bracket | Matches home decor (white) |

**Climate node firmware responsibilities:**
- Closed-loop PID temperature control (target ±0.5°C)
- Closed-loop PID humidity control (target ±3%RH)
- IR code learning: can learn your AC/heater remote codes during setup
- HVAC scheduling: pre-cools/heats room 30 min before bedtime
- Receives optimized setpoints from hub (based on ML sleep model)
- Reports real-time temp/RH to hub every 30 seconds
- Fallback: maintains last-known setpoint if hub is offline
- Safety: max temperature limits, auto-shutoff if sensor fails

### 4. Shade Controller (1-4 per system)

Motorized window shade controller with integrated dawn simulator. Automates darkness for sleep onset and natural light for waking.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-C3-MINI-1 | BLE 5, motor control, LED PWM |
| Motor Driver | TMC2209 (Stepstick) | Silent stepper driver for roller shade |
| Motor | NEMA 11 stepper (12V) | Smooth, precise shade positioning |
| LED Strip | 12V 60LED/m WWA (warm/cool/amber) | Dawn simulator, 3-channel |
| LED Driver | 3× PT4115 (1A LED driver) | Per-channel PWM dimming (0.1% resolution) |
| Light Sensor | VEML7700 | Ambient light feedback (close shade if >50lux at night) |
| Power | 12V DC barrel jack (2A) | Powers motor + LEDs |
| Enclosure | 200×40×30mm aluminum, clips to shade rail | Minimal, matches shade hardware |
| Limit Switches | 2× micro switches | Top/bottom shade position detection |

**Shade controller firmware responsibilities:**
- Stepper motor control: smooth acceleration/deceleration curve
- Position memory: learns shade open/closed positions via limit switches
- Dawn simulation: 30-min gradual LED warm-up before alarm (mimics sunrise spectrum)
- Blue-light blocking: shifts LED to amber after 9pm
- Receives commands from hub: open/close/set_position/set_dawn_time
- Reports position and ambient light to hub every 60 seconds
- Auto-close: if ambient light exceeds threshold during sleep, closes shade
- Manual override: physical button on unit for open/close

---

## Communication Protocol

### BLE 5 Mesh

All nodes use BLE 5 mesh for local communication. The nightstand hub acts as the provisioner and bridge to WiFi.

| Parameter | Value |
|-----------|-------|
| Protocol | BLE 5.0 Mesh (managed flooding) |
| Network | 1 mesh network per bedroom system |
| Provisioner | Nightstand Hub (ESP32-S3) |
| Nodes | Up to 6 (1 hub + 1 strip + 1 climate + up to 3 shades) |
| Range | ~10m (single room) |
| Interval | 5s (sleep data), 30s (environment), 60s (shade status) |
| Security | AES-CCM 128-bit encryption |
| Power | 0 dBm TX (sleep strip), +4 dBm (hub/climate/shade) |

### Mesh Message Types

```
SLEEP_DATA     (0x01) — Sleep strip → Hub: HR, RR, movement, snoring events
ENV_DATA       (0x02) — Climate → Hub: temp, RH, HVAC state
SHADE_STATUS   (0x03) — Shade → Hub: position, light, LED state
HUB_COMMAND    (0x04) — Hub → Climate/Shade: setpoints, open/close, dawn schedule
HUB_SYNC       (0x05) — Hub → All: clock sync, mesh params, OTA trigger
ALARM_TRIGGER  (0x06) — Hub → All: alarm event (sound + light + shade open)
ACK            (0x07) — Any → Hub: command acknowledgment
OTA_BLOCK      (0x08) — Hub → Any: firmware update chunk
```

### BLE GATT Service (Mobile App ↔ Hub)

| UUID | Characteristic | Access | Purpose |
|------|---------------|--------|---------|
| 0xFFC0 | Sleep Score | Read/Notify | Current sleep stage + score |
| 0xFFC1 | Environment | Read | Room temp, RH, CO2, light, noise |
| 0xFFC2 | Sound Control | Write | Select/adjust soundscape |
| 0xFFC3 | Climate Control | Write | Set temperature/humidity target |
| 0xFFC4 | Shade Control | Write | Open/close/set dawn time |
| 0xFFC5 | Alarm Config | Write | Set alarm window + sound |
| 0xFFC6 | System Status | Read/Notify | Node status, battery, errors |
| 0xFFC7 | WiFi Config | Write | SSID + password provisioning |

### MQTT Topics (Cloud)

| Topic | Direction | Content |
|-------|-----------|---------|
| `sleepsync/{device_id}/sleep_data` | Hub → Cloud | Sleep staging + vitals |
| `sleepsync/{device_id}/env_data` | Hub → Cloud | Environment readings |
| `sleepsync/{device_id}/alarm` | Hub → Cloud | Alarm triggered event |
| `sleepsync/{device_id}/daily_report` | Cloud → Hub | Morning sleep report |
| `sleepsync/{device_id}/env_setpoints` | Cloud → Hub | ML-optimized setpoints |
| `sleepsync/{device_id}/ota` | Cloud → Hub | OTA firmware blocks |
| `sleepsync/{device_id}/commands` | App → Hub | Remote commands |

---

## Data Payloads

### Sleep Data Packet (Sleep Strip → Hub, every 5s)

```
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ HR   │ HR_V │ RR   │ RR_V │ MOVE │ SNORE│ STAGE│ CONF │ BAT  │
│ 2B   │ 1B   │ 2B   │ 1B   │ 1B   │ 1B   │ 1B   │ 1B   │ 1B  │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘

HR:     Heart rate (BPM ×10, uint16) — 450 = 45.0 BPM
HR_V:   HR variability (0-255, unitless)
RR:     Respiration rate (breaths/min ×10, uint16) — 160 = 16.0/min
RR_V:   RR variability (0-255)
MOVE:   Movement intensity (0-255, derived from actigraphy)
SNORE: Snoring intensity (0-255, >100 = snoring event)
STAGE:  Predicted sleep stage (0=awake, 1=light, 2=deep, 3=REM)
CONF:   Stage confidence (0-255, ×100 = %)
BAT:    Battery percentage (0-100)
Total: 11 bytes
```

### Environment Data Packet (Climate → Hub, every 30s)

```
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ TEMP │ HUM  │ CO2  │ HVAC │ HTR  │ HUMD │ ERR  │
│ 2B   │ 2B   │ 2B   │ 1B   │ 1B   │ 1B   │ 1B   │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┘

TEMP:  Temperature (°C ×100, int16) — 2067 = 20.67°C
HUM:   Humidity (RH% ×100, uint16) — 4520 = 45.20%
CO2:   CO2 concentration (ppm, uint16) — 800 = 800ppm
HVAC:  HVAC state bitfield (bit0=cooling, bit1=heating, bit2=fan)
HTR:   Heater relay state (0=off, 1=on, 2=triac_level)
HUMD:  Humidifier relay state (0=off, 1=humidify, 2=dehumidify)
ERR:   Error bitfield (bit0=sensor_fail, bit1=relay_fail, bit2=comm_fail)
Total: 10 bytes
```

### Shade Status Packet (Shade → Hub, every 60s)

```
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ POS  │ LIGHT│ LED_W │ LED_A │ LED_C │ DAWN │ ERR  │
│ 1B   │ 2B   │ 1B   │ 1B   │ 1B   │ 4B   │ 1B   │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┘

POS:   Shade position (0-100%, 0=closed, 100=open)
LIGHT: Ambient light (lux, uint16 from VEML7700)
LED_W: Warm white LED level (0-255)
LED_A: Amber LED level (0-255)
LED_C: Cool white LED level (0-255)
DAWN:  Next dawn simulation start (Unix timestamp, uint32)
ERR:   Error bitfield (bit0=motor_stall, bit1=limit_fail, bit2=led_fail)
Total: 11 bytes
```

---

## AI / ML Pipeline

### 1. Sleep Staging (on-hub, TFLite Micro on ESP32-S3)

- **Input**: Rolling window of 60 sleep data packets (5 min at 5s intervals) → 11 features × 60 timesteps
- **Model**: 1D-CNN + BiLSTM + Attention, INT8 quantized, ~180KB
- **Output**: Sleep stage probability (4 classes: wake, light, deep, REM)
- **Accuracy**: ~82% vs clinical PSG (polysomnography) benchmark
- **Features used**: Heart rate + HRV, respiration rate + RR variability, movement intensity, snoring intensity
- **Triggers**: Stage transitions → hub adjusts environment in real-time

### 2. Environment Optimizer (cloud, PyTorch, runs nightly)

- **Input**: 30-day history of sleep stages + corresponding environment conditions
- **Model**: Bayesian optimization with Gaussian Process surrogate
- **Output**: Personalized optimal setpoints for temperature, humidity, and light per sleep stage
- **Feedback loop**: Each night's sleep quality (deep %, REM %, wake after sleep onset) updates the model
- **Cold start**: Uses population-level priors (18.3°C temp, 40-50% RH, 0 lux) for new users
- **Adapts to**: Season, mattress type, bedding, alcohol/caffeine (manual tags), menstrual cycle

### 3. Apnea & Snoring Detector (on-hub, TFLite Micro)

- **Input**: BCG signal + snoring intensity from sleep strip, 30s windows
- **Model**: 1D-CNN + LSTM, INT8 quantized, ~60KB
- **Output**: Apnea risk score (0-1), snoring event detection
- **Detection**: Central apnea (pause in breathing >10s), obstructive apnea (effort + no airflow signature), hypopnea (reduced breathing)
- **Action**: Logs events, generates weekly report, recommends clinical evaluation if AHI >5

### 4. Smart Alarm (on-hub, rule-based + ML wake-probability)

- **Input**: Current sleep stage + predicted stage transitions in next 30 min
- **Model**: Hidden Markov Model for stage transition prediction
- **Output**: Optimal wake time within alarm window
- **Logic**: Wakes you at the lightest sleep point within your alarm window (e.g., 6:30-7:00)
- **Result**: Reduced sleep inertia — you feel less groggy and more refreshed
- **Fallback**: If still in deep sleep at end of window, gentle wake with escalating light + sound

---

## Pin Assignments

### Nightstand Hub (ESP32-S3-WROOM-1-N8R8)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | I2S_0 BCLK/WS | UDA1334A audio DAC |
| GPIO2 | I2S_0 DOUT | UDA1334A data in |
| GPIO3 | I2S_0 DIN | SPH0645 mic data out |
| GPIO4/GPIO5 | I2S_0 MCLK | Audio clock (shared) |
| GPIO6/GPIO7 | I2C0 SDA/SCL | BME280, SCD40, TSL2591 |
| GPIO8 | SPI0 CLK | ILI9488 TFT + SD card |
| GPIO9 | SPI0 MOSI | ILI9488 TFT + SD card |
| GPIO10 | SPI0 MISO | ILI9488 TFT + SD card |
| GPIO11 | SPI0 CS0 | ILI9488 TFT CS |
| GPIO12 | TFT DC | Display data/command |
| GPIO13 | TFT RST | Display reset |
| GPIO14 | TFT BL | Display backlight PWM |
| GPIO15 | SD CS | MicroSD card CS |
| GPIO16/GPIO17 | UART0 TX/RX | Debug console (USB) |
| GPIO18 | WS2812B | Status LED data |
| GPIO19 | BTN_SNOOZE | Snooze button (active low) |
| GPIO20 | BTN_MODE | Mode button (active low) |
| GPIO21 | BTN_BRIGHT | Brightness button (active low) |
| GPIO38 | BATT_SENSE | Battery voltage ADC divider |
| GPIO39 | CHG_STATUS | Charge controller status |

### Sleep Strip (nRF52832-QFAA)

| Pin | Function | Connected To |
|-----|----------|-------------|
| P0.02/P0.03 | I2C0 SDA/SCL | LIS3DH accelerometer |
| P0.04 | SPI0 CS | LIS3DH (if SPI mode used) |
| P0.11 | HX711_1_SCK | First HX711 (FSR 1-4) clock |
| P0.12 | HX711_1_DOUT | First HX711 data |
| P0.13 | HX711_2_SCK | Second HX711 (FSR 5-8) clock |
| P0.14 | HX711_2_DOUT | Second HX711 data |
| P0.15 | LIS3DH_INT1 | Accelerometer interrupt (movement) |
| P0.16 | LIS3DH_INT2 | Accelerometer interrupt (free-fall) |
| P0.17 | CHG_EN | Qi charger enable |
| P0.18 | BATT_SENSE | Battery voltage ADC |
| P0.19 | LED_R | Status LED red |
| P0.20 | LED_G | Status LED green |
| P0.21 | LED_B | Status LED blue |
| P0.22 | QI_STATUS | Qi charge status |
| P0.24 | POWER_EN | Main power rail enable |

### Climate Node (ESP32-C3-MINI-1)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | I2C0 SDA/SCL | SHT40 temp/humidity sensor |
| GPIO2/GPIO3 | I2C1 SDA/SCL | SH1106 OLED display |
| GPIO4 | IR_LED | IR blaster LED (TSAL6100) |
| GPIO5 | RELAY_1 | Heater/fan relay |
| GPIO6 | RELAY_2 | Humidifier/dehumidifier relay |
| GPIO7 | TRIAC_PWM | BTA16 triac gate (dimmable heater) |
| GPIO8 | ZERO_CROSS | AC zero-crossing detector (for triac) |
| GPIO9 | BTN_MODE | Mode/config button |
| GPIO10 | LED_STATUS | Status LED (onboard) |

### Shade Controller (ESP32-C3-MINI-1)

| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO0/GPIO1 | I2C0 SDA/SCL | VEML7700 light sensor |
| GPIO2 | STEP | TMC2209 stepper step |
| GPIO3 | DIR | TMC2209 stepper direction |
| GPIO4 | EN | TMC2209 enable (active low) |
| GPIO5 | TMC_UART_TX | TMC2209 UART config (stealthChop) |
| GPIO6 | TMC_UART_RX | TMC2209 UART config |
| GPIO7 | LED_WW | PT4115 warm white PWM |
| GPIO8 | LED_AMBER | PT4115 amber PWM |
| GPIO9 | LED_COOL | PT4115 cool white PWM |
| GPIO10 | LIMIT_TOP | Top limit switch |
| GPIO11 | LIMIT_BOTTOM | Bottom limit switch |
| GPIO12 | BTN_MANUAL | Manual open/close button |
| GPIO13 | DIAG | TMC2209 diagnostic output |

---

## Power Architecture

```
Nightstand Hub:
  USB-C 5V ──► AP2112-3.3 (MCU + sensors + BLE)
              AP6212-1.8 (ESP32-S3 core)
              MCP73831 (LiPo charger, 2000mAh backup)
  Battery backup: ~8 hours (audio off), ~4 hours (audio low)

Sleep Strip:
  100mAh LiPo ──► TLV70233 (3.3V LDO, 300mA, IQ 1µA)
  Battery life: ~14 days (typical use, adaptive sampling)
  Qi wireless charging: 1W, ~2 hour full charge

Climate Node:
  5V USB-C wall adapter ──► AP2112-3.3 (MCU + sensors)
  Relays powered from 5V directly
  No battery (always wall-powered, non-critical)

Shade Controller:
  12V DC barrel jack ──► AP2112-3.3 (MCU + sensors)
  12V → TMC2209 (motor driver, ~800mA stepper)
  12V → PT4115 ×3 (LED drivers, 1A each)
  No battery (always wall-powered, non-critical)
```

---

## Smart Alarm — Detailed Operation

```
1. User sets alarm window: e.g., 6:30 - 7:00 AM
2. At 6:30, hub evaluates current sleep stage:
   a. If LIGHT sleep: trigger alarm immediately (optimal!)
   b. If REM sleep: trigger alarm within 5 min (acceptable)
   c. If DEEP sleep: delay, use HMM to predict next light sleep
3. Dawn simulator starts 30 min before window start:
   a. Shade controller begins LED warm-up (0→100% over 30 min)
   b. Spectrum shifts: amber → warm white → cool white (mimics sunrise)
4. When alarm triggers:
   a. Soundscape fades in over 60s (gentle → moderate)
   b. Hub display shows morning briefing (sleep score, weather, schedule)
   c. Shade opens to full position (natural light)
5. If user presses snooze:
   a. Sound stops, light stays at current level
   b. Re-evaluate in 5 min (won't re-enter deep sleep likely)
6. Hard alarm at end of window regardless of stage
```

---

## Environment Optimization — Detailed Operation

```
1. Bedtime approaching (30 min before set bedtime):
   a. Hub sends "pre-sleep" setpoints to climate node
   b. Climate node activates HVAC/cooling to reach target temp
   c. Shade controller closes shades fully
   d. Hub fades ambient to 0 lux, starts gentle soundscape

2. Sleep onset detected (stage = light for 5 min):
   a. Hub transitions to "sleep" setpoints
   b. Climate fine-tunes temp (typically lowers 1-2°C for deep sleep)
   c. Soundscape adjusts: reduces volume, shifts to pink/brown noise
   d. Hub display dims to off

3. Deep sleep detected:
   a. Climate maintains coolest temperature in optimal range
   b. Soundscape at minimum volume (noise masking only)
   c. Shade verifies fully closed (auto-corrects if light detected)

4. REM sleep detected:
   a. Climate slightly warms (0.5°C) — REM is thermoregulation-vulnerable
   b. No sound changes (REM sleep is sensitive to disruption)

5. Wake after sleep onset (WASO):
   a. If brief (<2 min): maintain environment, no action
   b. If prolonged (>5 min): slightly increase soundscape volume
   c. If room noise spike detected: increase noise masking

6. Morning wake:
   a. See Smart Alarm section above

7. Learning loop (nightly):
   a. Upload sleep quality metrics to cloud
   b. Cloud Bayesian optimizer updates setpoint recommendations
   c. Next night's targets are personalized based on what worked
```

---

## Weekly Sleep Report

Every Monday morning, the system generates a report:

- **Sleep Score** (0-100): Based on total sleep time, deep %, REM %, WASO, consistency
- **Deep Sleep**: Hours and percentage (target: 15-20% for adults)
- **REM Sleep**: Hours and percentage (target: 20-25%)
- **Sleep Latency**: Time to fall asleep (target: <20 min)
- **Wake Episodes**: Number and duration of nighttime awakenings
- **Snoring**: Total time snoring, apnea risk indicators
- **Environment**: Avg/peak temp, humidity, CO2, noise — with recommendations
- **Consistency**: Bedtime/wake time variance (consistency > sleep duration!)
- **Trends**: 4-week moving averages for all metrics
- **Recommendations**: Personalized tips based on your data patterns

---

## Firmware Overview

### Nightstand Hub (`firmware/nightstand-hub/hub_main.c`)

- FreeRTOS on ESP32-S3 with tasks: BLE mesh, WiFi/MQTT, audio engine, display, ML inference, alarm
- BLE mesh provisioner: configures sleep strip, climate node, shade controllers
- Audio engine: reads PCM sound files from SD card, mixes, applies fade/volume, outputs I2S
- ML inference: runs TFLite Micro sleep staging model every 5s on latest sleep data
- Display task: renders sleep stage ring, environment gauges, clock, morning report
- Smart alarm task: implements the alarm window logic described above
- WiFi task: MQTT publish/subscribe, OTA check, time sync (NTP)

### Sleep Strip (`firmware/sleep-strip/strip_main.c`)

- Bare-metal nRF52832 (no RTOS needed, deterministic timing)
- Main loop: sample BCG + accelerometer → preprocess → extract features → BLE transmit → sleep
- Adaptive sampling: 200Hz BCG + 50Hz accel during wake/light, 100Hz+25Hz during deep
- Power management: ~5.2µA deep sleep between samples, ~8mA active, ~50µA average

### Climate Node (`firmware/climate-node/climate_main.c`)

- ESP-IDF on ESP32-C3
- PID controller for temperature and humidity
- IR blaster learns + replays AC/heater codes
- Dual relay control (on/off for heater + humidifier)
- Triac dimming for resistive heater elements (zero-crossing detection)
- BLE mesh client: receives setpoints from hub, reports status

### Shade Controller (`firmware/shade-controller/shade_main.c`)

- ESP-IDF on ESP32-C3
- TMC2209 stepper driver: stealthChop mode for silent operation
- Position control: acceleration ramp → constant speed → deceleration → stop
- Limit switch calibration: auto-learns top/bottom positions on first power-up
- Dawn simulator: 3-channel LED PWM with 30-min sunrise program
- BLE mesh client: receives commands from hub, reports position/light

---

## Cloud Software

### FastAPI Backend (`software/dashboard/backend/main.py`)

- REST API for sleep data, environment data, reports, configuration
- MQTT bridge to receive real-time data from hubs
- SQLite database for history (upgradable to TimescaleDB for scale)
- Sleep score calculation engine
- Weekly report generation (cron job)
- OTA firmware management
- User authentication (JWT)

### ML Pipeline (`software/ml-pipeline/`)

- `train_sleep_staging.py` — 1D-CNN+BiLSTM+Attention sleep stager
- `train_apnea.py` — Apnea/snoring event detector
- `env_optimizer.py` — Bayesian environment optimization

---

## Mobile App (React Native)

- **Sleep Score**: Today's score + 7/30-day trend
- **Sleep Stages**: Visual hypnogram (wake/light/deep/REM bar chart)
- **Environment**: Current room conditions + history
- **Smart Alarm**: Set alarm window, choose soundscape
- **Soundscape**: Select/preview/adjust white/pink/brown noise, nature sounds
- **Climate Control**: Set target temp/humidity per sleep stage
- **Shade Control**: Manual open/close, dawn time, light threshold
- **Weekly Report**: AI-generated sleep improvement insights
- **Setup Wizard**: BLE provisioning, node discovery, calibration
- **Health Insights**: Snoring trends, apnea risk, circadian rhythm score

---

## Bill of Materials (Summary)

| Node | Est. Cost (qty 1) | Notes |
|------|-------------------|-------|
| Nightstand Hub | ~$35 | ESP32-S3 + audio + sensors + display |
| Sleep Strip | ~$28 | nRF52832 + FSRs + HX711 + Qi |
| Climate Node | ~$18 | ESP32-C3 + sensors + relays + IR |
| Shade Controller | ~$22 | ESP32-C3 + stepper + LEDs + driver |
| **Total (1 hub + 1 strip + 1 climate + 1 shade)** | **~$103** | Retail BOM, qty 1 |
| **Total (1 hub + 1 strip + 1 climate + 2 shades)** | **~$125** | Two windows |

---

## Safety & Privacy

- **No camera in the bedroom** — SleepSync uses BCG + actigraphy, never visual monitoring
- **No cloud-required for operation** — All critical functions (sleep tracking, alarm, climate, shade) work without internet
- **Local-first data** — 7-day buffer on SD card, cloud upload is optional
- **Encrypted BLE mesh** — AES-CCM 128-bit, no unencrypted data in transit
- **Encrypted MQTT** — TLS 1.3 for all cloud communication
- **No raw BCG stored** — Only extracted features (HR, RR, movement) are transmitted or stored
- **Safety limits** — Climate node has hardware max temperature limits, auto-shutoff on sensor failure
- **Fire safety** — All relay/triac outputs have thermal fuses, max duty cycle limits

---

## Getting Started

### Hardware Assembly

1. Flash firmware to all nodes (see `scripts/flash_all.sh`)
2. Assemble nightstand hub (see `docs/assembly.md`)
3. Place sleep strip under pillow (connectors facing headboard)
4. Mount climate node on wall near bed, plug in USB-C
5. Install shade controller on window shade rail (see `docs/shade_install.md`)
6. Connect 12V power to shade controller

### Software Setup

1. Install SleepSync mobile app
2. Power on nightstand hub — it creates a BLE advertising network
3. App discovers hub, prompts for WiFi credentials (provisioned via BLE)
4. Hub connects to WiFi, each node auto-joins the BLE mesh
5. Run calibration (strip placement, shade limits, HVAC IR learning)
6. Set your alarm window and bedtime
7. Sleep! The system learns and optimizes from night 1

---

## License

MIT — build it, sell it, improve it.

---

*Part of the [Devices](https://github.com/jayis1/Devices) project. Invented by [jayis1](https://github.com/jayis1).*