# SoundNest

**AI-powered home acoustic intelligence and sound management system.** Hears what matters, masks what doesn't, protects your hearing — autonomously.

---

## What It Does

SoundNest is a 4-node system that transforms your home's acoustic environment:

1. **Detects** critical sounds — doorbell, smoke alarm, crying baby, glass break, knocking — and alerts you instantly via haptics, phone, or visual flash
2. **Masks** unwanted noise — adaptive pink/brown/white noise, nature soundscapes that shift in real-time to match the intrusion
3. **Tracks** your daily sound exposure — protects your hearing with dose metrics, time-weighted averages, and peak alerts
4. **Classifies** every sound event in your home — ML identifies 50+ sound categories and builds an acoustic timeline
5. **Manages** tinnitus — generates personalized masking tones matched to your tinnitus frequency profile
6. **Creates** privacy zones — directional masking prevents conversations from being overheard in adjacent rooms

All nodes communicate over Sub-GHz mesh (no WiFi dependency for critical alerts). A hub node bridges to WiFi/cloud for dashboard, ML pipeline, and mobile app.

### The Problem It Solves

- **100M+** Americans live with chronic noise exposure affecting health (stress, cardiovascular, cognitive)
- **50M** Americans have tinnitus; 20M seek treatment; masking is the #1 non-pharmacological intervention
- **1.5B** people globally have hearing loss; many miss critical sounds (alarms, doorbells, crying)
- **Sleep disruption** from noise costs an estimated $50B+ annually in lost productivity
- **Apartment dwellers** have zero smart tools for managing neighbor noise, traffic, construction
- **Open offices** destroy focus; no adaptive masking exists
- **White noise machines** are dumb — static output, no adaptation, one size fits all
- **No system** integrates sound detection, masking, exposure tracking, and critical alerting

SoundNest fixes all of this. Install the hardware, set preferences, and it manages your acoustic environment better than you can.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SOUNDNEST SYSTEM                                │
│                                                                          │
│  ┌──────────────────┐   Sub-GHz    ┌───────────────────┐              │
│  │ ROOM ACOUSTIC     │◄───────────►│                    │              │
│  │ SENSOR            │   868MHz    │                    │              │
│  │ (per room)        │   mesh     │                    │              │
│  │ 4× MEMS mic       │            │    HUB NODE        │              │
│  │ SPL + spectrum    │            │    (ESP32-S3 +      │── WiFi6 ──► Cloud
│  │ Sound classifier  │            │     nRF52840)      │              Dashboard
│  │ Temp/humidity     │            │                     │              + ML
│  └──────────────────┘            │    Audio DSP        │              Pipeline
│                                   │    3.5" TFT         │              + Alerts
│  ┌──────────────────┐            │    Speaker           │
│  │ ROOM ACOUSTIC    │◄──────────►│    SD storage        │              + TTS
│  │ SENSOR 2         │   Sub-GHz  │                     │
│  │ (bedroom)        │   mesh     │                     │── BLE5 ───► Mobile
│  └──────────────────┘            │                     │              App
│                                   │                     │              (React
│  ┌──────────────────┐            │                     │              Native)
│  │ SMART MASKING    │◄──────────►│                     │
│  │ SPEAKER          │   Sub-GHz  └─────────┬───────────┘
│  │ (per room)       │   mesh               │ Sub-GHz mesh
│  │ DAC + 5W amp     │                     │ (up to 8 sensors
│  │ Adaptive noise   │                     │  + 4 speakers
│  │ Nature sounds    │                     │  per hub)
│  └──────────────────┘                     │
│                                           │
│  ┌──────────────────┐                     │
│  │ WEARABLE SOUND   │◄────────────────────┘
│  │ TAG              │   Sub-GHz + BLE
│  │ (per person)     │
│  │ Mic + haptic     │
│  │ Accel + LED      │
│  │ Sound dose       │
│  └──────────────────┘
│
│  ┌──────────────────────────────────────────────────────────────────┐
│  │                    CLOUD / EDGE SOFTWARE                        │
│  │  ┌────────────┐  ┌──────────────┐  ┌────────────────────────┐  │
│  │  │ Dashboard  │  │ ML Pipeline │  │ Mobile App             │  │
│  │  │ (FastAPI)  │  │ (PyTorch)   │  │ (React Native)         │  │
│  │  │ Acoustic   │  │ Sound event │  │ Push alerts            │  │
│  │  │ timeline   │  │ classification│ │ Sound dose tracker     │  │
│  │  │ Dose viz   │  │ Noise source│  │ Masking control        │  │
│  │  │ Config     │  │ Tinnitus    │  │ Preference wizard       │  │
│  │  │ Alerts     │  │ fingerprint  │  │ Tinnitus tuner         │  │
│  │  └────────────┘  └──────────────┘  └────────────────────────┘  │
│  └──────────────────────────────────────────────────────────────────┘
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges Sub-GHz mesh to WiFi/BLE/cloud. Runs local audio ML inference.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU (Primary) | ESP32-S3-WROOM-1-N8R8 | Audio DSP, WiFi 6, ML inference, 8MB PSRAM |
| MCU (Radio) | nRF52840 | BLE 5.0 mesh + Sub-GHz coordinator |
| Sub-GHz Radio | SX1262 | 868MHz LoRa mesh to all nodes |
| Display | 3.5" IPS TFT (ILI9488) | Acoustic timeline, sound events, dose meter |
| Audio Codec | ES8388 | ADC/DAC for hub speaker + mic |
| Speaker | 3W full-range (TDM-0303K0P) | Local alarms, notifications, TTS |
| Microphone | SPH0645LM4H-6 | Local sound pickup at hub |
| Storage | 16MB Flash + microSD (FAT32) | Audio samples, ML models, event log |
| RTC | DS3231 | Accurate timestamps for events |
| Power | 5V USB-C + 2000mAh Li-Po | Keeps running during power outage |
| Charging | TP4056 | Li-Po charge management |
| Power Path | MP28167 buck | 3.3V rail from USB/battery |
| USB | USB-C (USB2.0) | Power + OTA + serial debug |
| LEDs | RGB WS2812B ×2 | Status, alert level |
| Beeper | Piezo buzzer (PS1240) | Local alarm for critical events |
| Expansion | Qwiic ×2, 4× GPIO on header | Future sensors, relay control |

**Pin Assignments (ESP32-S3):**

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO0 | BOOT | Boot button |
| GPIO1 | I2S_BCLK | ES8388 audio clock |
| GPIO2 | I2S_LRCLK | ES8388 L/R clock |
| GPIO3 | I2S_DIN | ES8388 ADC data in |
| GPIO4 | I2S_DOUT | ES8388 DAC data out |
| GPIO5 | SPI_CLK | SD card + display |
| GPIO6 | SPI_MOSI | SD card + display |
| GPIO7 | SPI_MISO | SD card |
| GPIO8 | CS_SD | SD card chip select |
| GPIO9 | CS_TFT | TFT display chip select |
| GPIO10 | DC_TFT | TFT data/command |
| GPIO11 | BL_TFT | TFT backlight (PWM) |
| GPIO12 | SDA | I2C (ES8388, DS3231, Qwiic) |
| GPIO13 | SCL | I2C clock |
| GPIO14 | SPK_EN | Speaker amp enable |
| GPIO15 | PIEZO | Piezo buzzer PWM |
| GPIO16 | LED_DATA | WS2812B RGB LEDs |
| GPIO17 | VBUS_SENSE | USB power detect |
| GPIO18 | BAT_SENSE | Battery voltage ADC |
| GPIO35 | MIC_IRQ | Hub microphone interrupt |
| GPIO36 | BUTTON | User button |
| GPIO37 | SX1262_IRQ | Sub-GHz radio interrupt |
| GPIO38 | SX1262_RST | Sub-GHz radio reset |
| GPIO39 | SX1262_BUSY | Sub-GHz radio busy |
| GPIO40 | SX1262_CS | Sub-GHz radio SPI CS |
| GPIO41 | SPI2_CLK | SX1262 SPI clock |
| GPIO42 | SPI2_MOSI | SX1262 SPI MOSI |
| GPIO43 | SPI2_MISO | SX1262 SPI MISO |
| GPIO44 | nRF_UART_TX | UART to nRF52840 |
| GPIO45 | nRF_UART_RX | UART from nRF52840 |
| GPIO46 | nRF_RST | nRF52840 reset |
| GPIO47 | CHG_STAT | TP4056 charge status |
| GPIO48 | SDCD | SD card detect |

**Pin Assignments (nRF52840):**

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | SX1262_CS | Sub-GHz SPI CS |
| P0.03 | SX1262_IRQ | Sub-GHz interrupt |
| P0.04 | SX1262_RST | Sub-GHz reset |
| P0.05 | SX1262_BUSY | Sub-GHz busy |
| P0.06 | SPI_CLK | Sub-GHz SPI clock |
| P0.07 | SPI_MOSI | Sub-GHz SPI MOSI |
| P0.08 | SPI_MISO | Sub-GHz SPI MISO |
| P0.09 | UART_TX | UART to ESP32-S3 |
| P0.10 | UART_RX | UART from ESP32-S3 |
| P0.11 | BLE_ANT | BLE antenna |
| P0.13 | LED1 | Status LED |
| P0.14 | LED2 | Status LED |
| P0.15 | BUTTON | DFU/button |
| P0.18 | QWIIC_SDA | I2C expansion |
| P0.19 | QWIIC_SCL | I2C expansion |
| P0.20 | nRF_RST | ESP32 reset (bidirectional) |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler for up to 12 nodes)
- Audio ML inference (ESP-NN accelerated sound classification)
- Data aggregation and time-series buffering (5-minute windows, 1-second granularity)
- WiFi uplink to MQTT broker (QoS 1, TLS 1.3)
- BLE GATT server for mobile app (Nordic UART Service)
- TFT dashboard rendering (LVGL)
- Local alarm triggers (piezo + speaker + display flash)
- OTA update distribution to all nodes
- Tinnitus masking tone generation (personalized frequency profiles)
- Privacy masking mode coordination (multi-speaker directional masking)
- Sound dose calculation and exposure tracking

---

### 2. Room Acoustic Sensor (2-6 per system, one per room)

The ears. Each sensor node has a 4-microphone array for sound source localization, SPL measurement, spectral analysis, and local sound event classification via TinyML.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 | BLE 5.0 + Sub-GHz mesh, TinyML |
| Sub-GHz Radio | SX1262 | 868MHz LoRa mesh |
| Microphones | 4× SPH0645LM4H-6 | MEMS I2S mic array for sound localization |
| Mic Amp | MAX9814 | Preamplifier for ambient pickup |
| SPL Sensor | I2S digital SPL (built into SPH0645) | Sound pressure level measurement |
| Temp/Humidity | SHT40-AD1B | Temperature + humidity (affects acoustics) |
| Light Sensor | VEML7700 | Ambient light (activity proxy) |
| PIR Motion | AM312 | Room occupancy detection |
| LED | WS2812B ×1 | Status + local alert flash |
| Power | CR123A ×2 (3V) or USB-C | 6+ months on batteries, or continuous USB |
| Voltage Reg | AP2112K-3.3 | 3.3V LDO |
| Battery Monitor | nRF ADC | Battery voltage tracking |
| Antenna | 868MHz PCB trace + 2.4GHz PCB trace | Sub-GHz + BLE |

**Pin Assignments (nRF52840):**

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | MIC1_BCLK | I2S bit clock (shared) |
| P0.03 | MIC1_LRCLK | I2S L/R clock (shared) |
| P0.04 | MIC1_DOUT | Mic 1 data out |
| P0.05 | MIC2_DOUT | Mic 2 data out |
| P0.06 | MIC3_DOUT | Mic 3 data out |
| P0.07 | MIC4_DOUT | Mic 4 data out |
| P0.08 | I2C_SDA | SHT40 + VEML7700 |
| P0.09 | I2C_SCL | I2C clock |
| P0.10 | PIR_OUT | AM312 PIR motion |
| P0.11 | SX1262_CS | Sub-GHz SPI CS |
| P0.12 | SX1262_IRQ | Sub-GHz interrupt |
| P0.13 | SX1262_RST | Sub-GHz reset |
| P0.14 | SX1262_BUSY | Sub-GHz busy |
| P0.15 | SPI_CLK | Sub-GHz SPI clock |
| P0.16 | SPI_MOSI | Sub-GHz SPI MOSI |
| P0.17 | SPI_MISO | Sub-GHz SPI MISO |
| P0.18 | LED_DATA | WS2812B RGB LED |
| P0.19 | VBAT_SENSE | Battery voltage divider |
| P0.20 | USB_DETECT | USB-C power detect |
| P0.22 | ENABLE | Power enable for mics (power gating) |
| P0.23 | BUTTON | Setup/reset button |
| P0.25 | DEBUG_TX | Serial debug (optional) |
| P0.26 | DFU_BUTTON | Boot/DFU button |

**Microphone Array Geometry:**

The 4 MEMS microphones are arranged in a square pattern with 40mm spacing on the PCB:
```
    MIC1 ───── 40mm ───── MIC2
     │                       │
    40mm                   40mm
     │                       │
    MIC3 ───── 40mm ───── MIC4
```

This gives a ±15° sound source localization accuracy for frequencies up to 4kHz, covering the speech band and most environmental sounds.

**Sensor firmware responsibilities:**
- 4-mic array capture at 16kHz/16-bit (I2S, continuous)
- Sound source localization (TDOA beamforming on nRF52840)
- SPL measurement (A-weighted, C-weighted, Z-weighted)
- 1/3-octave spectral analysis (32 bands, 20Hz-20kHz)
- Local TinyML sound event classification (40 classes, 2-second windows)
- Room occupancy detection (PIR + sound activity)
- Temperature/humidity reporting (for acoustic correction)
- Light level reporting (activity proxy)
- Low-power sleep between measurement windows
- Sub-GHz mesh uplink to hub (TDMA schedule)
- BLE provisioning (for initial WiFi setup via mobile app)

**TinyML Sound Event Classes (40 categories):**

| Category | Events |
|----------|--------|
| Alarm | Smoke alarm, CO alarm, burglar alarm, car alarm, timer |
| Door | Doorbell, door knock, door open/close |
| Human | Speech, crying baby, cough, sneeze, laugh, shout |
| Animal | Dog bark, cat meow, bird chirp |
| Kitchen | Microwave, blender, dishwasher, kettle, faucet |
| Home | Vacuum, washer, dryer, fan, AC unit, TV, music |
| Traffic | Car horn, siren, engine, motorcycle, bicycle bell |
| Nature | Rain, thunder, wind, running water |
| Work | Phone ring, notification, keyboard typing |
| Alert | Glass break, crash/impact, explosion, gunfire |

---

### 3. Smart Masking Speaker (1-4 per system, one per room)

The voice. Generates adaptive noise masking, nature soundscapes, tinnitus masking tones, and privacy masking. Each speaker can produce independent masking profiles per room.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-S3-MINI-1 | Audio processing + WiFi |
| Sub-GHz Radio | SX1262 | 868MHz LoRa mesh |
| DAC | PCM5102A | 32-bit/384kHz audio DAC for high-quality masking |
| Audio Amp | MAX98306 | 2× 3W Class-D (stereo for directional masking) |
| Speakers | 2× TDM-0303K0P | 3W full-range speakers for stereo masking |
| Microphone | SPH0645LM4H-6 | Reference mic for adaptive feedback |
| Flash | 8MB PSRAM + 16MB Flash | Audio buffers, masking sound library |
| Power | 5V USB-C | Continuous power required |
| Voltage Reg | AP2112K-3.3 + SY8089 (1.8V) | 3.3V and 1.8V rails |
| LED | WS2812B ×1 | Status indicator |
| IR LED | IR333C ×2 | Night-mode indicator |
| Enclosure | 3D-printed ABS | Reflective parabolic for directional masking |

**Pin Assignments (ESP32-S3-MINI-1):**

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO0 | BOOT | Boot button |
| GPIO1 | I2S_DAC_BCLK | PCM5102A bit clock |
| GPIO2 | I2S_DAC_LRCLK | PCM5102A L/R clock |
| GPIO3 | I2S_DAC_DOUT | PCM5102A data out |
| GPIO4 | DAC_FMT | PCM5102A format (I2S) |
| GPIO5 | DAC_XSMT | PCM5102A soft mute |
| GPIO6 | I2S_MIC_BCLK | SPH0645 mic bit clock |
| GPIO7 | I2S_MIC_LRCLK | SPH0645 mic L/R clock |
| GPIO8 | I2S_MIC_DIN | SPH0645 mic data in |
| GPIO9 | AMP_ENABLE | MAX98306 enable |
| GPIO10 | SX1262_CS | Sub-GHz SPI CS |
| GPIO11 | SX1262_IRQ | Sub-GHz interrupt |
| GPIO12 | SX1262_RST | Sub-GHz reset |
| GPIO13 | SX1262_BUSY | Sub-GHz busy |
| GPIO14 | SPI_CLK | Sub-GHz SPI clock |
| GPIO15 | SPI_MOSI | Sub-GHz SPI MOSI |
| GPIO16 | SPI_MISO | Sub-GHz SPI MISO |
| GPIO17 | LED_DATA | WS2812B RGB LED |
| GPIO18 | IR_LED1 | IR indicator LED |
| GPIO19 | IR_LED2 | IR indicator LED |
| GPIO21 | BUTTON | Mode button |
| GPIO38 | VBAT_SENSE | Not used (USB only) |

**Speaker Enclosure Design:**

The enclosure uses a parabolic reflector behind the speakers to create a directional sound pattern:

```
         ┌─────────────────┐
         │   Parabolic     │
         │   Reflector     │
         │  (ABS, 3D-      │
         │   printed)      │
    ┌────┤                 ├────┐
    │SPK1 │   Chamber      │SPK2│
    │ ◄───┤───────────────┤───►│
    └────┤                 ├────┘
         │   Electronics   │
         │   PCB mount     │
         └─────────────────┘
              ▼ ▼ ▼ ▼ ▼
         (Directional output)
```

This creates a ±30° sound cone vs. omnidirectional, allowing targeted masking in specific zones (e.g., masking noise at a window while keeping the rest of the room quiet).

**Speaker firmware responsibilities:**
- Receive masking commands from hub (via Sub-GHz or WiFi)
- Generate real-time noise masking (white, pink, brown, nature sounds)
- Adaptive volume control based on reference mic feedback
- Stereo spatial masking (left/right balance for directional masking)
- Tinnitus masking tone generation (personalized frequency profiles)
- Privacy masking mode (anti-eavesdrop directional noise)
- Sleep mode masking (gradual volume changes, fade-in/fade-out)
- Audio playback for alarms and notifications
- Local audio processing (ESP-NN accelerated)
- Firmware OTA update via hub

**Masking Modes:**

| Mode | Description | Use Case |
|------|-------------|----------|
| White noise | Equal energy per frequency | General masking, tinnitus |
| Pink noise | Equal energy per octave | Speech masking, focus |
| Brown noise | -6dB/octave rolloff | Sleep, deep relaxation |
| Nature: Rain | Synthesized rain + thunder | Sleep, relaxation |
| Nature: Stream | Synthesized flowing water | Focus, relaxation |
| Nature: Forest | Birds + wind + rustling | Focus, calm |
| Nature: Ocean | Waves + seagulls | Sleep, meditation |
| Tinnitus mask | Tuned narrowband noise | Tinnitus relief |
| Privacy mask | Directional speech-shaped noise | Prevent eavesdropping |
| Custom | User-uploaded sounds | Personal preference |

---

### 4. Wearable Sound Tag (1 per person)

The personal guardian. Worn as a badge clip or lanyard, it monitors your personal sound exposure, provides haptic alerts for critical sounds, and tracks daily dose metrics.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52832 | BLE 5.0 + proprietary 2.4GHz |
| Microphone | SPH0645LM4H-6 | Personal sound level monitoring |
| Haptic | ERM vibration motor (303-100) | Silent alerts for critical sounds |
| LED | RGB LED (APA106-2020) | Visual alert (color-coded by event type) |
| Accelerometer | LIS2DH12 | Activity detection, fall detection proxy |
| Battery | 100mAh Li-Po (3.7V) | 7-day life per charge |
| Charging | MCP73831 | USB-C Li-Po charger |
| Voltage Reg | AP2112K-3.3 | 3.3V LDO |
| Button | Tactile switch ×2 | Mute, pair, mode |
| Antenna | 2.4GHz PCB trace | BLE antenna |
| Enclosure | Injection-molded PC | 45×30×12mm badge |

**Pin Assignments (nRF52832):**

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | MIC_BCLK | I2S bit clock |
| P0.03 | MIC_LRCLK | I2S L/R clock |
| P0.04 | MIC_DOUT | SPH0645 data out |
| P0.05 | I2C_SDA | LIS2DH12 accelerometer |
| P0.06 | I2C_SCL | I2C clock |
| P0.07 | ACC_INT | LIS2DH12 interrupt |
| P0.09 | HAPTIC_PWM | ERM motor PWM drive |
| P0.10 | LED_DATA | APA106 RGB LED |
| P0.11 | CHG_STAT | MCP73831 charge status |
| P0.12 | VBAT_SENSE | Battery voltage divider |
| P0.13 | BUTTON1 | Mute/snooze button |
| P0.14 | BUTTON2 | Pair/mode button |
| P0.15 | MIC_EN | Microphone power enable |
| P0.16 | HAPTIC_EN | Haptic motor enable |
| P0.19 | USB_DETECT | USB-C power detect |
| P0.20 | SWDIO | Debug/programming |
| P0.21 | SWCLK | Debug/programming |

**Wearable firmware responsibilities:**
- Continuous sound level monitoring (1-second SPL samples, A-weighted)
- Sound dose accumulation (daily, weekly, monthly metrics)
- BLE connection to hub for real-time event alerts
- Haptic vibration patterns for critical sound alerts:
  - 1 buzz: Doorbell
  - 2 buzzes: Phone/alarm
  - 3 buzzes: Baby crying
  - Long buzz: Smoke/CO alarm
  - Double-long: Glass break / intrusion
- RGB LED color-coded alerts:
  - 🔴 Red: Emergency (fire, CO, intrusion)
  - 🟠 Orange: Urgent (doorbell, phone, alarm)
  - 🟡 Yellow: Info (appliance done, timer)
  - 🟢 Green: Masking active / all clear
  - 🔵 Blue: Connected / syncing
- Activity classification (sitting, walking, running via LIS2DH12)
- BLE provisioning and pairing
- 7-day battery life with smart wake/sleep
- OTA firmware update via hub

---

## Power Architecture

```
                          ┌─────────────┐
                          │  USB-C 5V   │
                          └──────┬──────┘
                                 │
                    ┌────────────┼────────────────┐
                    │            │                │
              ┌─────┴─────┐ ┌───┴───┐  ┌────────┴────────┐
              │ TP4056    │ │ MP28167│  │ AP2112K-3.3      │
              │ Charger   │ │ Buck  │  │ LDO (sensors)    │
              └─────┬─────┘ │3.3V/2A│  └────────┬────────┘
                    │       └───┬───┘           │
              ┌─────┴─────┐    │          ┌─────┴──────┐
              │ Li-Po     │    │          │ SHT40      │
              │ 2000mAh   │    │          │ VEML7700   │
              └─────┬─────┘    │          │ SPH0645    │
                    │          │          └────────────┘
                    │    ┌─────┴──────────────────────┐
                    │    │  3.3V Main Rail              │
                    │    │  ESP32-S3  nRF52840  SD      │
                    │    │  SX1262   Display  ES8388   │
                    │    └─────────────────────────────┘
                    │
              ┌─────┴─────┐
              │ Power Path │  (Automatic USB/battery switch)
              │ Management │
              └────────────┘
```

**Battery Life Estimates:**

| Node | Power Source | Battery Life |
|------|-------------|-------------|
| Hub | USB-C + 2000mAh Li-Po backup | Indefinite on USB; 8h on battery |
| Room Sensor (batteries) | 2× CR123A (3V) | 6 months (10-min intervals) |
| Room Sensor (USB-C) | USB-C | Indefinite |
| Masking Speaker | USB-C (5V/2A) | N/A (always powered) |
| Wearable Tag | 100mAh Li-Po | 7 days typical use |

---

## Communication Protocol

### Sub-GHz Mesh (868MHz, LoRa)

- **Modulation**: LoRa, SF7BW125 (urban), SF9BW125 (suburban), SF12BW125 (long range)
- **Frequency**: 868.1MHz (EU) / 915MHz (US) — configurable
- **Topology**: Star-with-mesh — hub is coordinator, sensors relay for distant nodes
- **TDMA Schedule**: Hub assigns 1-second slots per node, 10-second superframes
- **Encryption**: AES-128-CCM, per-node key derived from provisioning
- **Range**: 100m indoor (SF7), 500m indoor (SF12), 2km+ line-of-sight
- **Data Rate**: 5kbps (SF7) — sufficient for event reports, SPL readings, commands

### Packet Format

```
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ PREAMBLE │ SYNC │ LEN  │ SRC  │ DST  │ TYPE │ SEQ  │  PAY │  MIC │
│  4 bytes │ 2    │ 1    │ 2    │ 2    │ 1    │ 2    │ 0-64│ 4    │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘

TYPE values:
  0x01 = JOIN_REQ      (node → hub)
  0x02 = JOIN_ACK      (hub → node)
  0x03 = EVENT_REPORT  (sensor → hub)
  0x04 = SPL_REPORT    (sensor → hub, periodic)
  0x05 = MASKING_CMD   (hub → speaker)
  0x06 = ALERT_CMD     (hub → wearable)
  0x07 = CONFIG_UPDATE (hub → node)
  0x08 = OTA_BLOCK     (hub → node)
  0x09 = HEARTBEAT     (bidirectional)
  0x0A = DOSE_REPORT   (wearable → hub)
```

### BLE 5.0 (Mobile App ↔ Hub)

- **Service**: Nordic UART Service (NUS) for config + data
- **Custom Service**: SoundNest Service (UUID: 7E21...) for real-time alerts
  - Characteristic: Sound Event (notify) — sound classification results
  - Characteristic: SPL Level (notify) — real-time sound pressure
  - Characteristic: Dose (notify) — daily sound dose percentage
  - Characteristic: Masking Control (write) — start/stop/adjust masking
  - Characteristic: Config (read/write) — node configuration

### WiFi (Hub ↔ Cloud)

- **Protocol**: MQTT v5 over TLS 1.3
- **Broker**: Mosquitto (local) or AWS IoT Core (cloud)
- **Topics**:
  - `soundnest/{device_id}/events` — sound event detections
  - `soundnest/{device_id}/spl` — SPL time-series
  - `soundnest/{device_id}/dose` — sound dose updates
  - `soundnest/{device_id}/masking` — masking status
  - `soundnest/{device_id}/config` — configuration changes
  - `soundnest/{device_id}/ota` — firmware updates
  - `soundnest/{device_id}/cmd` — commands from cloud

---

## Firmware Architecture

### Hub Node Firmware

```
┌──────────────────────────────────────────────────┐
│                  HUB FIRMWARE                     │
│                  (ESP32-S3)                        │
│                                                   │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐ │
│  │ Mesh Mgr   │  │ Audio ML   │  │ MQTT Mgr   │ │
│  │ (Sub-GHz   │  │ (ESP-NN    │  │ (WiFi +    │ │
│  │  TDMA      │  │  sound     │  │  TLS 1.3)  │ │
│  │  scheduler)│  │  classif.)  │  │            │ │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘ │
│        │               │               │         │
│  ┌─────┴───────────────┴───────────────┴──────┐  │
│  │              Event Bus (FreeRTOS)            │  │
│  └─────┬───────────────┬───────────────┬──────┘  │
│        │               │               │         │
│  ┌─────┴──────┐ ┌─────┴──────┐ ┌─────┴──────┐  │
│  │ Display    │ │ Alarm Mgr  │ │ BLE GATT   │  │
│  │ (LVGL     │ │ (piezo +  │ │ (NUS +     │  │
│  │  TFT)     │ │  speaker + │ │  custom    │  │
│  │            │ │  display)  │ │  service)  │  │
│  └────────────┘ └────────────┘ └────────────┘  │
│                                                   │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐ │
│  │ Dose Calc  │ │ Masking    │ │ OTA Mgr    │ │
│  │ (TWA,      │ │ Engine     │ │ (delta +   │ │
│  │  peak      │ │ (noise     │ │  full)     │ │
│  │  tracking) │ │  synthesis)│ │            │ │
│  └────────────┘ └────────────┘ └────────────┘  │
└──────────────────────────────────────────────────┘
```

### Room Sensor Firmware

```
┌──────────────────────────────────────────┐
│           ROOM SENSOR FIRMWARE           │
│              (nRF52840)                   │
│                                          │
│  ┌────────────┐  ┌────────────────────┐  │
│  │ Mic Array  │  │ TinyML Sound       │  │
│  │ Driver     │─►│ Classifier         │  │
│  │ (4× I2S   │  │ (40 classes,       │  │
│  │  SPH0645)  │  │  2s windows,       │  │
│  │            │  │  int8 quantized)   │  │
│  └────────────┘  └────────┬───────────┘  │
│                           │              │
│  ┌────────────┐  ┌────────┴───────────┐  │
│  │ SPL Meter  │  │ Event Builder      │  │
│  │ (A/C/Z     │  │ (class + confidence│  │
│  │  weighted, │  │  + direction +     │  │
│  │  1s int)   │  │  SPL + timestamp)  │  │
│  └─────┬──────┘  └────────┬───────────┘  │
│        │                  │              │
│  ┌─────┴──────────────────┴──────────┐  │
│  │         Mesh TX Manager           │  │
│  │  (TDMA schedule, AES-128-CCM,     │  │
│  │   retry, ACK)                     │  │
│  └────────────────┬─────────────────┘  │
│                    │                    │
│  ┌────────────┐    │                    │
│  │ Power Mgr  │    │                    │
│  │ (sleep     │    │                    │
│  │  schedule, │    │                    │
│  │  wake on   │    │                    │
│  │  sound)    │    │                    │
│  └────────────┘    │                    │
│                    ▼                    │
│              Sub-GHz Radio              │
└──────────────────────────────────────────┘
```

---

## Cloud/Edge Software

### FastAPI Dashboard Backend

```
soundnest-dashboard/
├── app/
│   ├── main.py              # FastAPI app + lifespan
│   ├── config.py             # Settings (MQTT broker, DB, etc.)
│   ├── database.py           # SQLAlchemy + SQLite/TimescaleDB
│   ├── mqtt_client.py        # MQTT v5 client (asyncio)
│   ├── routers/
│   │   ├── events.py         # Sound event endpoints
│   │   ├── spl.py            # SPL time-series endpoints
│   │   ├── dose.py           # Sound dose endpoints
│   │   ├── masking.py        # Masking control endpoints
│   │   ├── nodes.py          # Node management endpoints
│   │   ├── config.py         # Configuration endpoints
│   │   └── auth.py           # User auth endpoints
│   ├── models/
│   │   ├── event.py          # SoundEvent model
│   │   ├── spl_reading.py    # SPLReading model
│   │   ├── dose_record.py    # DoseRecord model
│   │   ├── node.py           # Node model
│   │   └── user.py           # User model
│   ├── schemas/
│   │   ├── event.py          # Pydantic schemas
│   │   ├── spl.py            # SPL schemas
│   │   ├── dose.py           # Dose schemas
│   │   └── node.py           # Node schemas
│   ├── services/
│   │   ├── event_service.py  # Business logic
│   │   ├── spl_service.py    # SPL aggregation
│   │   ├── dose_service.py   # Dose calculations
│   │   ├── masking_service.py # Masking orchestration
│   │   ├── alert_service.py  # Alert routing
│   │   └── ml_service.py     # ML inference orchestration
│   └── ws/
│       ├── events.py         # WebSocket for real-time events
│       └── spl.py            # WebSocket for live SPL
├── alembic/
│   └── versions/             # DB migrations
├── requirements.txt
├── Dockerfile
└── docker-compose.yml
```

### ML Pipeline

```
soundnest-ml/
├── training/
│   ├── dataset/
│   │   ├── esc50/             # ESC-50 environmental sound dataset
│   │   ├── urbanse/           # UrbanSound8K dataset
│   │   ├── audioset/          # AudioSet ontology
│   │   └── custom/            # Custom recorded sounds
│   ├── feature_extraction.py  # Mel-spectrogram, MFCC, onset detection
│   ├── train_classifier.py    # Sound event classification (40 classes)
│   ├── train_localizer.py    # Sound source localization (TDOA)
│   ├── train_dose_predict.py # Sound dose prediction model
│   ├── train_masking.py       # Optimal masking recommendation
│   ├── quantize_model.py     # int8 quantization for TinyML
│   ├── convert_tflite.py     # TFLite Micro conversion
│   └── evaluate.py           # Model evaluation metrics
├── inference/
│   ├── hub_inference.py      # Hub-side inference (ESP32-S3)
│   ├── cloud_inference.py    # Cloud-side inference (GPU)
│   └── edge_inference.py     # Edge inference (RP2040 companion)
├── models/
│   ├── sound_classifier.tflite  # TinyML model for sensor
│   ├── sound_classifier.onnx   # Hub model
│   ├── sound_classifier.pt     # Training checkpoint
│   ├── localizer.pt            # Sound localization model
│   └── dose_predictor.pt       # Dose prediction model
├── configs/
│   ├── sensor_model.yaml     # TinyML model config
│   ├── hub_model.yaml        # Hub model config
│   └── cloud_model.yaml      # Cloud model config
└── requirements.txt
```

---

## Sound Dose & Exposure Tracking

SoundNest tracks personal sound exposure using OSHA and WHO standards:

### Metrics

| Metric | Formula | Limit |
|--------|---------|-------|
| TWA (Time-Weighted Average) | OSHA 8-hour TWA | 85 dBA (action level), 90 dBA (PEL) |
| Daily Dose % | D = 100 × (T_actual / T_allowed) | 100% = maximum daily exposure |
| Peak Exposure | Maximum SPL measured | 140 dBA (OSHA peak) |
| Equivalent Level (Leq) | Energy-averaged SPL | WHO: 70 dBA annual average |
| Noise Dose Rate | % per hour at current level | Tracks exposure trajectory |

### Daily Dose Calculation

```c
// Simplified dose calculation
float calculate_dose_percent(float spl_dba, float duration_hours) {
    // Reference: 85 dBA for 8 hours = 100% dose (OSHA)
    // 3 dB exchange rate (ISO) or 5 dB (OSHA)
    // Using 3 dB exchange rate (more conservative)
    float reference_level = 85.0f;  // dBA
    float reference_hours = 8.0f;
    float exchange_rate = 3.0f;  // dB
    
    float allowed_hours = reference_hours * 
        powf(2.0f, (reference_level - spl_dba) / exchange_rate);
    
    return (duration_hours / allowed_hours) * 100.0f;
}

// Example: 100 dBA for 1 hour = 100% dose
// (allowed time at 100 dBA = 8 * 2^((85-100)/3) = 8 * 0.125 = 1 hour)
```

### Alert Thresholds

| Dose % | Alert Level | Action |
|--------|------------|--------|
| < 50% | Green | No action needed |
| 50-100% | Yellow | Warning on phone + tag LED |
| 100-200% | Orange | Suggest hearing protection, reduce exposure |
| > 200% | Red | Mandatory hearing protection alert |

---

## Tinnitus Masking

SoundNest includes a personalized tinnitus masking system:

1. **Audiometric Test** (mobile app): Users perform a simple tone-matching exercise to identify their tinnitus frequency (typically 2-10kHz)
2. **Frequency Profile**: The app creates a profile: tinnitus frequency, bandwidth, and loudness relative to ambient
3. **Masking Algorithm**: The hub generates a narrowband noise mask centered 1 octave below the tinnitus frequency, with adjustable bandwidth and volume
4. **Adaptive Level**: Masking volume adjusts automatically based on ambient noise (louder when room is quiet, softer when ambient provides natural masking)
5. **Sleep Mode**: Gradual fade-out over 30 minutes as the user falls asleep

---

## Bill of Materials

### Hub Node BOM

| # | Component | Part | Qty | Unit Price | Total | Notes |
|---|-----------|------|-----|-----------|-------|-------|
| 1 | MCU Primary | ESP32-S3-WROOM-1-N8R8 | 1 | $5.50 | $5.50 | 8MB PSRAM, 16MB Flash |
| 2 | MCU Radio | nRF52840-module | 1 | $4.20 | $4.20 | BLE 5.0 + SPI |
| 3 | Sub-GHz Radio | SX1262 | 1 | $3.80 | $3.80 | 868/915MHz |
| 4 | Display | 3.5" IPS TFT (ILI9488) | 1 | $6.50 | $6.50 | 480×320 |
| 5 | Audio Codec | ES8388 | 1 | $1.20 | $1.20 | ADC/DAC |
| 6 | Speaker | TDM-0303K0P 3W | 1 | $1.50 | $1.50 | Full range |
| 7 | Microphone | SPH0645LM4H-6 | 1 | $2.80 | $2.80 | MEMS I2S |
| 8 | RTC | DS3231 | 1 | $2.10 | $2.10 | ±2ppm accuracy |
| 9 | SD Card Slot | MicroSD (push-push) | 1 | $0.50 | $0.50 | FAT32 logging |
| 10 | Battery | Li-Po 2000mAh | 1 | $4.50 | $4.50 | Backup power |
| 11 | Charger IC | TP4056 | 1 | $0.30 | $0.30 | Li-Po CC/CV |
| 12 | Buck Converter | MP28167 | 1 | $1.20 | $1.20 | 3.3V/2A |
| 13 | USB-C | USB-C 2.0 connector | 1 | $0.40 | $0.40 | Power + data |
| 14 | LEDs | WS2812B-2020 | 2 | $0.15 | $0.30 | RGB status |
| 15 | Piezo | PS1240 | 1 | $0.20 | $0.20 | Buzzer |
| 16 | Crystal | 32.768kHz | 1 | $0.15 | $0.15 | RTC crystal |
| 17 | Passives | Capacitors, resistors | 1 | $1.50 | $1.50 | Decoupling, etc. |
| 18 | Connectors | Qwiic ×2, header | 1 | $1.00 | $1.00 | Expansion |
| 19 | PCB | 4-layer FR4 | 1 | $3.00 | $3.00 | 100×80mm |
| 20 | Enclosure | Injection-molded ABS | 1 | $4.00 | $4.00 | Custom mold |
| | | | | **Total** | **$45.35** | |

### Room Acoustic Sensor BOM

| # | Component | Part | Qty | Unit Price | Total | Notes |
|---|-----------|------|-----|-----------|-------|-------|
| 1 | MCU | nRF52840-module | 1 | $3.50 | $3.50 | BLE + Sub-GHz |
| 2 | Sub-GHz Radio | SX1262 | 1 | $3.80 | $3.80 | 868/915MHz |
| 3 | Microphones | SPH0645LM4H-6 | 4 | $2.80 | $11.20 | MEMS I2S array |
| 4 | Temp/Humidity | SHT40-AD1B | 1 | $1.50 | $1.50 | ±0.2°C, ±1.8% RH |
| 5 | Light Sensor | VEML7700 | 1 | $1.20 | $1.20 | Ambient light |
| 6 | PIR Motion | AM312 | 1 | $0.50 | $0.50 | Occupancy |
| 7 | LED | WS2812B-2020 | 1 | $0.15 | $0.15 | RGB status |
| 8 | Battery Holder | CR123A ×2 holder | 1 | $0.40 | $0.40 | 6V → 3.3V |
| 9 | LDO | AP2112K-3.3 | 1 | $0.30 | $0.30 | 3.3V/600mA |
| 10 | Antenna | 868MHz PCB trace | 1 | $0.00 | $0.00 | On PCB |
| 11 | Passives | Capacitors, resistors | 1 | $1.00 | $1.00 | Decoupling, etc. |
| 12 | PCB | 4-layer FR4 | 1 | $2.50 | $2.50 | 60×60mm |
| 13 | Enclosure | Injection-molded PC | 1 | $2.50 | $2.50 | Ceiling/wall mount |
| | | | | **Total** | **$28.55** | |

### Smart Masking Speaker BOM

| # | Component | Part | Qty | Unit Price | Total | Notes |
|---|-----------|------|-----|-----------|-------|-------|
| 1 | MCU | ESP32-S3-MINI-1 | 1 | $3.00 | $3.00 | Audio processing |
| 2 | Sub-GHz Radio | SX1262 | 1 | $3.80 | $3.80 | 868/915MHz |
| 3 | DAC | PCM5102A | 1 | $2.50 | $2.50 | 32-bit/384kHz |
| 4 | Audio Amp | MAX98306 | 1 | $1.80 | $1.80 | 2× 3W Class-D |
| 5 | Speakers | TDM-0303K0P 3W | 2 | $1.50 | $3.00 | Full range, stereo |
| 6 | Microphone | SPH0645LM4H-6 | 1 | $2.80 | $2.80 | Reference mic |
| 7 | Memory | 8MB PSRAM + 16MB Flash | 1 | $1.50 | $1.50 | Audio buffers |
| 8 | LDO | AP2112K-3.3 | 1 | $0.30 | $0.30 | 3.3V |
| 9 | Buck | SY8089 | 1 | $0.40 | $0.40 | 1.8V |
| 10 | LED | WS2812B-2020 | 1 | $0.15 | $0.15 | RGB status |
| 11 | IR LEDs | IR333C | 2 | $0.10 | $0.20 | Night indicator |
| 12 | USB-C | USB-C 2.0 connector | 1 | $0.40 | $0.40 | Power |
| 13 | Passives | Capacitors, resistors | 1 | $1.50 | $1.50 | Decoupling, etc. |
| 14 | PCB | 4-layer FR4 | 1 | $2.50 | $2.50 | 100×50mm |
| 15 | Enclosure | 3D-printed ABS | 1 | $3.00 | $3.00 | Parabolic reflector |
| | | | | **Total** | **$26.05** | |

### Wearable Sound Tag BOM

| # | Component | Part | Qty | Unit Price | Total | Notes |
|---|-----------|------|-----|-----------|-------|-------|
| 1 | MCU | nRF52832-module | 1 | $2.80 | $2.80 | BLE 5.0 |
| 2 | Microphone | SPH0645LM4H-6 | 1 | $2.80 | $2.80 | MEMS I2S |
| 3 | Accelerometer | LIS2DH12 | 1 | $0.80 | $0.80 | Activity detection |
| 4 | Haptic | ERM 303-100 | 1 | $0.50 | $0.50 | Vibration motor |
| 5 | LED | APA106-2020 | 1 | $0.10 | $0.10 | RGB indicator |
| 6 | Battery | 100mAh Li-Po | 1 | $2.00 | $2.00 | 3.7V |
| 7 | Charger | MCP73831 | 1 | $0.40 | $0.40 | USB-C charging |
| 8 | LDO | AP2112K-3.3 | 1 | $0.30 | $0.30 | 3.3V |
| 9 | USB-C | USB-C 2.0 connector | 1 | $0.40 | $0.40 | Charging |
| 10 | Buttons | Tactile switch | 2 | $0.05 | $0.10 | Mute, pair |
| 11 | Passives | Capacitors, resistors | 1 | $0.80 | $0.80 | Decoupling, etc. |
| 12 | PCB | 4-layer FR4 | 1 | $2.00 | $2.00 | 45×30mm |
| 13 | Enclosure | Injection-molded PC | 1 | $2.00 | $2.00 | Badge/clip form |
| | | | | **Total** | **$15.90** | |

### Full System BOM (Typical 3-Room Deployment)

| Node | Qty | Unit Cost | Total |
|------|-----|-----------|-------|
| Hub | 1 | $45.35 | $45.35 |
| Room Acoustic Sensor | 3 | $28.55 | $85.65 |
| Smart Masking Speaker | 2 | $26.05 | $52.10 |
| Wearable Sound Tag | 2 | $15.90 | $31.80 |
| | | **Total** | **$214.90** |

---

## API Specification

### REST API Endpoints

```
# Sound Events
GET    /api/v1/events                     # List sound events (paginated)
GET    /api/v1/events/{id}                 # Get event details
GET    /api/v1/events/stats/summary       # Summary statistics
GET    /api/v1/events/stats/by-class       # Events by sound class
GET    /api/v1/events/stats/by-hour         # Events by hour of day

# SPL Readings
GET    /api/v1/spl                          # List SPL readings
GET    /api/v1/spl/live                     # Current SPL per room
GET    /api/v1/spl/history                  # Historical SPL data
GET    /api/v1/spl/stats                    # SPL statistics (Leq, Lmax, L10, L50, L90)

# Sound Dose
GET    /api/v1/dose/today                   # Today's dose for each person
GET    /api/v1/dose/history                  # Historical dose data
GET    /api/v1/dose/alerts                  # Dose alerts
POST   /api/v1/dose/tinnitus-profile        # Set tinnitus frequency profile

# Masking Control
POST   /api/v1/masking/start                # Start masking in room(s)
POST   /api/v1/masking/stop                 # Stop masking
PUT    /api/v1/masking/settings              # Update masking settings
GET    /api/v1/masking/status                # Current masking status

# Node Management
GET    /api/v1/nodes                         # List all nodes
GET    /api/v1/nodes/{id}                    # Get node details
PUT    /api/v1/nodes/{id}/config             # Update node config
POST   /api/v1/nodes/{id}/ota               # Trigger OTA update
DELETE /api/v1/nodes/{id}                    # Remove node

# Configuration
GET    /api/v1/config                        # Get system config
PUT    /api/v1/config                        # Update system config
GET    /api/v1/config/alert-rules            # Get alert rules
PUT    /api/v1/config/alert-rules            # Update alert rules
GET    /api/v1/config/masking-profiles       # Get masking profiles
PUT    /api/v1/config/masking-profiles       # Update masking profiles

# Authentication
POST   /api/v1/auth/register                 # Register user
POST   /api/v1/auth/login                    # Login
POST   /api/v1/auth/refresh                  # Refresh token
```

### WebSocket Channels

```
ws://hub:8000/ws/events          # Real-time sound events
ws://hub:8000/ws/spl             # Real-time SPL per room
ws://hub:8000/ws/dose            # Real-time dose updates
ws://hub:8000/ws/alerts          # Real-time alerts
```

---

## Mobile App (React Native)

### Screens

1. **Dashboard** — Real-time acoustic overview: current SPL per room, active alerts, masking status, daily dose
2. **Sound Timeline** — Chronological event log with spectrograms, classification labels, and playback
3. **Room Detail** — Per-room SPL history, noise heatmap, top sound classes, masking control
4. **Sound Dose** — Personal dose tracker: current %, daily history, peak exposures, weekly trends
5. **Masking Control** — Start/stop/adjust masking per room, choose masking mode, set schedules
6. **Tinnitus Tuner** — Audiometric test, frequency matching, masking tone preview, personalized profile
7. **Alert Settings** — Configure which sound events trigger alerts, alert destinations (phone, tag, display)
8. **Node Management** — Add/remove nodes, update firmware, check battery levels
9. **Reports** — Weekly/monthly acoustic reports, noise source breakdown, dose trends
10. **Setup Wizard** — Initial system setup, room assignment, sensor placement guide

---

## Directory Structure

```
sound-nest/
├── README.md                          # This file
├── schematic/
│   ├── hub-node/
│   │   ├── hub-node.kicad_pro         # KiCad project
│   │   ├── hub-node.kicad_sch         # Schematic
│   │   ├── hub-node.kicad_pcb         # PCB layout
│   │   └── README.md                   # Schematic notes
│   ├── room-sensor/
│   │   ├── room-sensor.kicad_pro
│   │   ├── room-sensor.kicad_sch
│   │   ├── room-sensor.kicad_pcb
│   │   └── README.md
│   ├── masking-speaker/
│   │   ├── masking-speaker.kicad_pro
│   │   ├── masking-speaker.kicad_sch
│   │   ├── masking-speaker.kicad_pcb
│   │   └── README.md
│   └── wearable-tag/
│       ├── wearable-tag.kicad_pro
│       ├── wearable-tag.kicad_sch
│       ├── wearable-tag.kicad_pcb
│       └── README.md
├── firmware/
│   ├── hub-node/
│   │   ├── CMakeLists.txt
│   │   ├── main/
│   │   │   ├── main.c               # Entry point
│   │   │   ├── mesh_manager.c       # Sub-GHz mesh coordinator
│   │   │   ├── mesh_manager.h
│   │   │   ├── audio_ml.c           # Sound classification (ESP-NN)
│   │   │   ├── audio_ml.h
│   │   │   ├── mqtt_manager.c       # WiFi MQTT client
│   │   │   ├── mqtt_manager.h
│   │   │   ├── ble_gatt.c           # BLE GATT server
│   │   │   ├── ble_gatt.h
│   │   │   ├── display.c            # LVGL TFT display
│   │   │   ├── display.h
│   │   │   ├── alarm_manager.c      # Local alarms
│   │   │   ├── alarm_manager.h
│   │   │   ├── dose_calculator.c     # Sound dose math
│   │   │   ├── dose_calculator.h
│   │   │   ├── masking_engine.c      # Noise synthesis
│   │   │   ├── masking_engine.h
│   │   │   ├── tinnitus.c            # Tinnitus masking
│   │   │   ├── tinnitus.h
│   │   │   └── ota_manager.c         # OTA updates
│   │   └── components/
│   │       ├── es8388/               # Audio codec driver
│   │       ├── ili9488/              # Display driver
│   │       ├── sx1262/               # Sub-GHz radio driver
│   │       └── esp_nn/               # Neural network acceleration
│   ├── room-sensor/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.c               # Entry point
│   │   │   ├── mic_array.c           # 4-mic I2S capture
│   │   │   ├── mic_array.h
│   │   │   ├── sound_classifier.c    # TinyML inference
│   │   │   ├── sound_classifier.h
│   │   │   ├── spl_meter.c           # SPL measurement
│   │   │   ├── spl_meter.h
│   │   │   ├── localizer.c           # Sound localization (TDOA)
│   │   │   ├── localizer.h
│   │   │   ├── sensors.c             # Temp/humidity/light/PIR
│   │   │   ├── sensors.h
│   │   │   ├── mesh_client.c          # Sub-GHz mesh client
│   │   │   ├── mesh_client.h
│   │   │   └── power_manager.c        # Sleep/wake management
│   │   └── lib/
│   │       ├── sph0645/               # MEMS mic driver
│   │       ├── sht40/                  # Temp/humidity driver
│   │       ├── veml7700/              # Light sensor driver
│   │       └── tflite_micro/          # TensorFlow Lite Micro
│   ├── masking-speaker/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.c               # Entry point
│   │   │   ├── audio_output.c         # PCM5102A + MAX98306 driver
│   │   │   ├── audio_output.h
│   │   │   ├── noise_generator.c      # White/pink/brown noise
│   │   │   ├── noise_generator.h
│   │   │   ├── nature_synth.c          # Rain/stream/forest/ocean
│   │   │   ├── nature_synth.h
│   │   │   ├── adaptive_masking.c      # Volume feedback loop
│   │   │   ├── adaptive_masking.h
│   │   │   ├── reference_mic.c         # SPL feedback from room
│   │   │   ├── reference_mic.h
│   │   │   ├── mesh_client.c           # Sub-GHz mesh client
│   │   │   └── mesh_client.h
│   │   └── lib/
│   │       ├── pcm5102a/               # DAC driver
│   │       └── max98306/               # Amplifier driver
│   ├── wearable-tag/
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.c               # Entry point
│   │   │   ├── sound_dose.c           # SPL dose tracking
│   │   │   ├── sound_dose.h
│   │   │   ├── haptic_alerts.c         # Vibration patterns
│   │   │   ├── haptic_alerts.h
│   │   │   ├── led_alerts.c            # RGB LED patterns
│   │   │   ├── led_alerts.h
│   │   │   ├── accelerometer.c         # Activity detection
│   │   │   ├── accelerometer.h
│   │   │   └── ble_client.c           # BLE connection to hub
│   │   └── lib/
│   │       ├── lis2dh12/               # Accelerometer driver
│   │       └── sph0645/                # MEMS mic driver
│   └── common/
│       ├── protocol/
│       │   ├── mesh_packet.h           # Sub-GHz packet definitions
│       │   ├── mesh_packet.c           # Packet encode/decode
│       │   ├── mesh_crypto.c           # AES-128-CCM encryption
│       │   ├── mesh_crypto.h
│       │   ├── ble_protocol.h          # BLE GATT definitions
│       │   ├── mqtt_topics.h           # MQTT topic definitions
│       │   └── sound_events.h           # Sound event enum definitions
│       ├── dsp/
│       │   ├── fft.h                   # FFT implementation (arm_math)
│       │   ├── fft.c
│       │   ├── mel_spectrogram.h       # Mel-spectrogram computation
│       │   ├── mel_spectrogram.c
│       │   ├── spl.h                   # SPL calculation (A/C/Z weighting)
│       │   ├── spl.c
│       │   ├── tdoa.h                  # Time-difference-of-arrival
│       │   └── tdoa.c
│       └── hal/
│           ├── sx1262.h                # SX1262 radio HAL
│           ├── sx1262.c
│           ├── power.h                 # Power management abstraction
│           └── power.c
├── hardware/
│   └── bom/
│       ├── hub-node.csv
│       ├── room-sensor.csv
│       ├── masking-speaker.csv
│       └── wearable-tag.csv
├── software/
│   ├── dashboard/
│   │   ├── app/
│   │   │   ├── main.py
│   │   │   ├── config.py
│   │   │   ├── database.py
│   │   │   ├── mqtt_client.py
│   │   │   ├── routers/
│   │   │   ├── models/
│   │   │   ├── schemas/
│   │   │   ├── services/
│   │   │   └── ws/
│   │   ├── alembic/
│   │   ├── requirements.txt
│   │   ├── Dockerfile
│   │   └── docker-compose.yml
│   ├── ml-pipeline/
│   │   ├── training/
│   │   │   ├── dataset/
│   │   │   ├── feature_extraction.py
│   │   │   ├── train_classifier.py
│   │   │   ├── train_localizer.py
│   │   │   ├── train_dose_predict.py
│   │   │   ├── train_masking.py
│   │   │   ├── quantize_model.py
│   │   │   ├── convert_tflite.py
│   │   │   └── evaluate.py
│   │   ├── inference/
│   │   │   ├── hub_inference.py
│   │   │   ├── cloud_inference.py
│   │   │   └── edge_inference.py
│   │   ├── models/
│   │   ├── configs/
│   │   └── requirements.txt
│   └── mobile-app/
│       ├── App.tsx
│       ├── package.json
│       ├── src/
│       │   ├── screens/
│       │   │   ├── DashboardScreen.tsx
│       │   │   ├── TimelineScreen.tsx
│       │   │   ├── RoomDetailScreen.tsx
│       │   │   ├── DoseScreen.tsx
│       │   │   ├── MaskingScreen.tsx
│       │   │   ├── TinnitusScreen.tsx
│       │   │   ├── AlertSettingsScreen.tsx
│       │   │   ├── NodesScreen.tsx
│       │   │   ├── ReportsScreen.tsx
│       │   │   └── SetupWizard.tsx
│       │   ├── components/
│       │   │   ├── SPLMeter.tsx
│       │   │   ├── SoundEventCard.tsx
│       │   │   ├── DoseGauge.tsx
│       │   │   ├── MaskingControls.tsx
│       │   │   ├── RoomCard.tsx
│       │   │   └── TimelineChart.tsx
│       │   ├── services/
│       │   │   ├── api.ts
│       │   │   ├── ble.ts
│       │   │   ├── notifications.ts
│       │   │   └── websocket.ts
│       │   ├── store/
│       │   │   ├── eventsSlice.ts
│       │   │   ├── splSlice.ts
│       │   │   ├── doseSlice.ts
│       │   │   ├── maskingSlice.ts
│       │   │   └── nodesSlice.ts
│       │   └── utils/
│       │       ├── doseCalc.ts
│       │       ├── soundClasses.ts
│       │       └── maskingProfiles.ts
│       └── android/
│           └── ios/
├── docs/
│   ├── architecture.md               # System architecture
│   ├── api.md                         # API specification
│   ├── protocol.md                    # Communication protocol spec
│   ├── sound-events.md                # Sound event classification catalog
│   ├── acoustic-primer.md             # Acoustics fundamentals
│   ├── tinnitus-guide.md              # Tinnitus masking theory
│   └── assembly.md                    # Assembly instructions
└── scripts/
    ├── setup.sh                       # Initial system setup
    ├── calibrate_spl.py               # SPL calibration script
    ├── calibrate_mics.py              # Mic array calibration
    ├── tinnitus_test.py               # Tinnitus frequency matching
    ├── deploy_dashboard.sh            # Deploy FastAPI backend
    ├── train_model.sh                  # Train ML models
    └── flash_firmware.sh               # Flash firmware to nodes
```

---

## Key Use Cases

### 1. Apartment Noise Management
> Live in an apartment with noisy neighbors. SoundNest detects footstep noise from above, automatically activates pink noise masking in your bedroom, and adjusts volume in real-time based on the intrusion level. You sleep peacefully.

### 2. Home Safety for Hearing-Impaired
> Hard of hearing? SoundNest detects your doorbell, smoke alarm, baby crying, and phone ringing — then vibrates your wearable tag and flashes your room sensors. Never miss a critical sound again.

### 3. Tinnitus Relief
> Suffer from tinnitus? SoundNest generates a personalized masking tone matched to your exact tinnitus frequency, adaptively adjusting volume as ambient noise changes. Fall asleep easier and focus better.

### 4. Focus Mode for Remote Work
> Working from home with kids, traffic, and construction? SoundNest detects distracting sounds and activates directional masking in your office zone. ML learns which sounds bother you most and preemptively masks them.

### 5. Noise Exposure Tracking
> Commute through a noisy city? Wear the Sound Tag to track your daily noise dose. Get alerts when you've exceeded safe exposure levels. Weekly reports show your noise sources and trends.

### 6. Privacy Mode
> Having a confidential conversation? Activate privacy mode and SoundNest generates directional speech-shaped masking from the Smart Speakers, preventing eavesdropping from adjacent rooms.

---

## Regulatory & Safety

- **FCC Part 15**: Sub-GHz and BLE radios comply with FCC regulations for unlicensed devices
- **IEC 61672**: SPL measurements comply with sound level meter standards
- **OSHA 29 CFR 1910.95**: Sound dose calculations follow OSHA occupational noise standards
- **WHO Guidelines**: Environmental noise guidelines for daily exposure
- **IEC 60601**: Medical device considerations for hearing assistance features
- **GDPR**: All audio processing happens locally; only event classifications (not raw audio) are sent to cloud
- **Privacy**: Raw audio is NEVER recorded, stored, or transmitted. Only ML classification results (sound class, confidence, SPL, direction) leave the device

---

*Invented by [jayis1](https://github.com/jayis1). Part of the [Devices](https://github.com/jayis1/Devices) collection.*