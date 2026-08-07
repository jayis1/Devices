# GuideSync — AI-Powered Spatial Awareness, Navigation & Visual Assistance System for the Blind & Visually Impaired

> **A multi-node wearable IoT system that gives blind and visually impaired users real-time spatial awareness, obstacle avoidance, text reading, indoor navigation, and emergency fall detection — combining smart glasses with on-device scene understanding, a sensor-packed smart cane, BLE nav beacons, a haptic wrist band, and a vision hub that bridges to the cloud. Built for the 2.2B+ people living with vision impairment worldwide, including 43M who are blind.**

---

## 1. Overview

GuideSync is a full-stack wearable IoT system that replaces the limitations of a white cane alone with a multi-sensor, AI-driven spatial awareness network worn on the head, wrist, and hand, plus BLE beacons placed throughout the home and frequently-visited buildings. The **Smart Glasses** (ESP32-S3 + OV5640 camera + VL53L5CX 8×8 ToF array) run on-device scene understanding (YOLOv8-nano), reading text on signs/menus/labels, detecting crosswalks and traffic signals, and classifying obstacles in the user's path — delivering audio guidance through bone conduction transducers that don't block environmental sound. The **Smart Cane** (nRF52840 + ultrasonic + ToF) detects ground-level obstacles, drop-offs, and stairs that glasses can't see, with haptic feedback in the handle. The **Nav Beacons** (×N, nRF52840, BLE) enable indoor navigation and landmark identification in homes, offices, and public buildings. The **Haptic Band** (nRF52840) provides wrist-based navigation turn-by-turn haptics, fall detection, and an emergency SOS. The **Vision Hub** (ESP32-S3, Wi-Fi + 4G LTE) coordinates the BLE star network, runs heavier ML inference (OCR, face recognition), bridges to the cloud, and triggers emergency alerts.

**Key outcomes:**
- **Real-time obstacle detection** — SceneNet (YOLOv8-nano, 80 classes) runs on glasses ESP32-S3 in <300 ms, with 8×8 ToF array providing depth-verified proximity warnings
- **Text reading** — TextReader (EAST + CRNN OCR) reads signs, menus, product labels, and medication instructions aloud via bone conduction
- **Indoor navigation** — NavNet LSTM triangulates position from BLE beacon RSSI fingerprints (±1.2 m accuracy in beaconed buildings), with turn-by-turn haptic navigation
- **Crosswalk & traffic signal detection** — CrosswalkNet CNN detects crosswalks, pedestrian signals (walk/don't walk), and countdown timers
- **Ground-level hazard detection** — Smart cane ultrasonic + downward ToF detects drop-offs, stairs, curbs, and low obstacles below the glasses' field of view
- **Fall detection & SOS** — Haptic band IMU fall detection (96% sensitivity, <0.3 FP/day) with automatic emergency contact + 911 dispatch via 4G LTE
- **Bone conduction audio** — Open-ear design preserves environmental hearing (critical for blind users who rely on auditory cues)
- **Privacy-first** — all scene understanding runs on-device; no video leaves the glasses unless the user explicitly requests cloud OCR or face recognition

### Problem Statement

**2.2 billion people** worldwide live with vision impairment, including **43 million who are blind** and **295 million with moderate-to-severe impairment** (WHO, 2024). Navigation, reading, and environmental awareness — tasks that sighted people take for granted — are daily challenges. The white cane, invented in 1921, remains the primary mobility tool a century later. Screen readers help with digital content but not the physical world. GPS navigation works outdoors but fails indoors. Existing smart glasses (OrCam, Envision) are single-device solutions with limited sensor fusion, no ground-level coverage, no indoor navigation infrastructure, and no emergency safety net.

GuideSync is the first multi-node system that fuses **head-level vision** (glasses), **ground-level sensing** (cane), **ambient infrastructure** (beacons), and **wrist-level safety** (band) into a coordinated spatial awareness network — with a 6-model ML pipeline running across edge and cloud.

---

## 2. System Architecture

```
                         ┌──────────────────────────────────────────────┐
                         │              CLOUD BACKEND                   │
                         │  FastAPI + MQTT + InfluxDB + PostgreSQL      │
                         │  6-model ML pipeline (GPU inference)         │
                         │  SceneNet · ObstacleNet · TextReader         │
                         │  NavNet · CrosswalkNet · FallNet             │
                         │  OTA firmware · Map beacon registry          │
                         │  Emergency dispatch (Twilio + 911)           │
                         │  Familiar face database (encrypted)          │
                         └──────────────────────────────────────────────┘
                                          ▲▼ MQTT / HTTPS
                         ┌──────────────────────────────────────────────┐
                         │              VISION HUB                      │
                         │  ESP32-S3 + Wi-Fi 2.4 GHz + BLE 5.0           │
                         │  4G LTE cellular backup (SIM7000)             │
                         │  Local edge inference (TFLite-Micro)          │
                         │  BLE star coordinator · OCR (on-hub)          │
                         │  BME280 · DS3231 RTC · microSD · USB-C/PoE    │
                         └──────────────────────────────────────────────┘
                              ▲         ▲         ▲         ▲
                              │BLE 5.0  │BLE 5.0  │BLE 5.0  │BLE 5.0
                              │         │         │         │
                    ┌─────────┴─────────┴─────────┴─────────┴────────┐
                    │                                                     │
              ┌─────┴──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
              │ SMART GLASSES  │  │ SMART    │  │ HAPTIC   │  │ NAV      │
              │                │  │ CANE     │  │ BAND     │  │ BEACON×N │
              │ ESP32-S3       │  │ nRF52840 │  │ nRF52840 │  │ nRF52840 │
              │ OV5640 camera  │  │ HC-SR04  │  │ DRV2605L │  │ BLE adv  │
              │ VL53L5CX ToF   │  │ VL53L0X  │  │ ICM-42688│  │ iBeacon  │
              │ ICM-42688 IMU  │  │ ICM-42688│  │ SOS btn  │  │ CR2032   │
              │ Bone conduct.  │  │ DRV2605L │  │ LiPo     │  │ 18 mo    │
              │ I²S mic        │  │ LiPo     │  │          │  │          │
              │ SceneNet CNN   │  │          │  │          │  │          │
              │ CrosswalkNet   │  │          │  │ FallNet  │  │          │
              │ LiPo 800 mAh   │  │ LiPo     │  │          │  │          │
              └────────────────┘  └──────────┘  └──────────┘  └──────────┘
```

### Data Flow

1. **Smart Glasses** (head-mounted) continuously capture scene images (OV5640) + depth grid (VL53L5CX 8×8 ToF) → on-device SceneNet (YOLOv8-nano) detects and classifies objects, obstacles, and hazards in the user's path → bone conduction audio describes the scene ("person ahead 2 meters", "chair at 1 o'clock") → CrosswalkNet detects crosswalks and pedestrian signals → TextReader (user-triggered) reads signs/labels aloud
2. **Smart Cane** (hand-held) sweeps ground level → ultrasonic sensor detects obstacles 2 cm–4 m ahead → downward-angled ToF detects drop-offs, stairs, and curbs → haptic motor in handle vibrates with direction-specific patterns (left/right/center obstacle) → IMU tracks cane swing for gait analysis
3. **Nav Beacons** (×N, wall-mounted) broadcast BLE advertisements with unique IDs and landmark metadata → glasses + band RSSI fingerprints → NavNet LSTM computes indoor position → haptic band provides turn-by-turn navigation (left wrist buzz = turn left, right = turn right, double-pulse = arrive)
4. **Haptic Band** (wrist-worn) provides navigation haptics + fall detection (IMU 1D-CNN) → on fall detection, sends BLE alert to Hub → Hub dispatches emergency SMS/call via 4G LTE to emergency contacts + 911 with GPS location
5. **Vision Hub** (pocket/bag-carried) coordinates BLE star network, runs heavier OCR inference for TextReader, bridges all telemetry to cloud via Wi-Fi/MQTT with 4G LTE backup, manages OTA firmware, and stores encrypted familiar-face embeddings
6. **Cloud** runs full 6-model ML pipeline — SceneNet retraining, NavNet beacon map learning, TextReader adaptation, emergency dispatch, map/beacon registry, familiar face database
7. **Mobile App** (sighted caregiver or user with VoiceOver) configures beacons, manages emergency contacts, reviews navigation history, calibrates sensors, and shares location with caregivers

---

## 3. Hardware Nodes

### 3.1 Vision Hub / Gateway

| Component | Part | Notes |
|-----------|------|-------|
| SoC | ESP32-S3-WROOM-1-N16R8 | 16 MB flash, 8 MB PSRAM, dual-core 240 MHz, vector instructions for CNN |
| BLE | Built-in BLE 5.0 | Star network coordinator (central role) |
| Wi-Fi | Built-in 2.4 GHz | Cloud connectivity (MQTT/HTTPS) |
| Cellular Backup | SIM7000A | 4G LTE Cat-M1, embedded SIM, emergency alerts when Wi-Fi unavailable |
| Temp/Humidity | BME280 | Ambient monitoring (device health, condensation prevention) |
| RTC | DS3231SN | Battery-backed, ±2 ppm (critical for offline logging) |
| Power | USB-C 5V / PoE (IEEE 802.3af) | TPS25940 eFuse, 3.3V regulator |
| Battery | LiPo 3.7V 2000 mAh | Portable operation ~12 hours (pocket-carried) |
| Storage | microSD slot | Local data buffering (14-day capacity), OCR model cache |
| LEDs | SK6812 RGB ×3 | Status: BLE, Wi-Fi/cellular, cloud |
| Buzzer | CMT-8543S-SMT | Audible alerts (fall, SOS, low battery) |
| Antenna | PCB trace BLE + Wi-Fi | Internal |
| Cellular Antenna | SMA paddle | 4G LTE |
| Charger | MCP73871 | USB-C + battery management |

**Pin Assignments (ESP32-S3):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | BME280 SDA | I²C data |
| GPIO5 | BME280 SCL | I²C clock |
| GPIO6 | DS3231 SDA | I²C data (shared bus) |
| GPIO7 | DS3231 SCL | I²C clock (shared bus) |
| GPIO8 | SD card MOSI | SPI |
| GPIO9 | SD card MISO | SPI |
| GPIO10 | SD card SCK | SPI |
| GPIO11 | SD card CS | SPI CS |
| GPIO12 | LED data | SK6812 |
| GPIO13 | Buzzer | PWM |
| GPIO14 | SIM7000 TX | UART2 TX (cellular) |
| GPIO15 | SIM7000 RX | UART2 RX (cellular) |
| GPIO16 | SIM7000 PWRKEY | Cellular power control |
| GPIO17 | Battery voltage | ADC |
| GPIO18 | USB power detect | Input only |
| GPIO43 | USB TX | UART0 (debug) |
| GPIO44 | USB RX | UART0 (debug) |

### 3.2 Smart Glasses

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R2 | 8 MB flash, 2 MB PSRAM, vector instructions for CNN |
| Camera | OV5640 | 5 MP, auto-focus, 72° FOV, DVP parallel interface (scene capture for SceneNet + CrosswalkNet + TextReader) |
| ToF Array | VL53L5CX | 8×8 (64-zone) ToF, 4 m range, 15 Hz, I²C — depth grid for ObstacleNet proximity verification |
| IMU | ICM-42688 | 6-axis accel + gyro, head orientation tracking, step counting |
| Bone Conduction | Bone conduction transducer pair | Audio output without blocking ears (open-ear design preserves environmental hearing) |
| Audio Amp | MAX98357A | I²S Class-D amplifier for bone conduction drivers |
| Microphone | ICS-43434 I²S MEMS | Voice commands ("read text", "describe scene", "where am I") |
| Edge AI | TFLite-Micro | SceneNet int8 (~3.8 MB), CrosswalkNet int8 (~220 KB), ObstacleNet int8 (~80 KB) |
| BLE | Built-in BLE 5.0 | Peripheral to Hub |
| Battery | LiPo 3.7V 800 mAh | ~6 hours continuous operation |
| Charger | MCP73871 | USB-C charging + battery management |
| LEDs | SK6812 RGB ×1 | Status indicator |
| Enclosure | 3D-printed PA12 (nylon) | Glasses frame, IP54, lightweight <60 g electronics |
| Frame | TR90 spectacle frame | Bone conduction drivers on temples, camera on bridge, ToF center-front |

**Pin Assignments (ESP32-S3):**

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO4 | OV5640 D7 | Camera parallel bus |
| GPIO5 | OV5640 D6 | Camera |
| GPIO6 | OV5640 D5 | Camera |
| GPIO7 | OV5640 D4 | Camera |
| GPIO8 | OV5640 D3 | Camera |
| GPIO9 | OV5640 D2 | Camera |
| GPIO10 | OV5640 D1 | Camera |
| GPIO11 | OV5640 D0 | Camera |
| GPIO12 | OV5640 VSYNC | Camera |
| GPIO13 | OV5640 HREF | Camera |
| GPIO14 | OV5640 PCLK | Camera |
| GPIO15 | OV5640 XCLK | Camera (20 MHz) |
| GPIO16 | OV5640 SIOC | Camera SCCB (I²C clock) |
| GPIO17 | OV5640 SIOD | Camera SCCB (I²C data) |
| GPIO18 | VL53L5CX SDA | I²C data (ToF array) |
| GPIO19 | VL53L5CX SCL | I²C clock (ToF array) |
| GPIO20 | ICM-42688 SDA | I²C data (IMU) |
| GPIO21 | ICM-42688 SCL | I²C clock (IMU) |
| GPIO22 | I²S mic BCLK | Voice command mic bit clock |
| GPIO23 | I²S mic LRCLK | Voice command mic word select |
| GPIO24 | I²S mic DATA | Voice command mic data in |
| GPIO25 | I²S amp BCLK | Bone conduction audio bit clock |
| GPIO26 | I²S amp LRCLK | Bone conduction audio word select |
| GPIO27 | I²S amp DATA | Bone conduction audio data out |
| GPIO28 | Battery voltage | ADC |
| GPIO29 | LED data | SK6812 |
| GPIO30 | USB power detect | Input only |
| GPIO43 | USB TX | UART0 (debug) |
| GPIO44 | USB RX | UART0 (debug) |

### 3.3 Smart Cane

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, 1 MB flash, 256 KB RAM, BLE 5.0 |
| Ultrasonic | HC-SR04 | 2 cm–4 m range, 15° beam, ground-level obstacle detection |
| ToF (downward) | VL53L0X | Single-zone ToF, 30 cm–2 m, angled 45° downward for drop-off/stair/curb detection |
| IMU | ICM-42688 | Cane swing tracking, tap detection (tap cane = "describe location"), gait analysis |
| Haptic Driver | DRV2605L | Haptic motor driver, 123 waveforms, direction-specific vibration patterns |
| Haptic Motor | ERM motor 10mm | In cane handle, vibration for obstacle direction + navigation cues |
| BLE | Built-in BLE 5.0 | Peripheral to Hub |
| Battery | LiPo 3.7V 500 mAh | ~20 hours operation (duty-cycled sensors) |
| Charger | MCP73871 | USB-C charging |
| Enclosure | Carbon fiber cane shaft | Electronics in handle + sensor pod near tip, IP54 |
| LED | Single LED | Status (connection, battery) |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | VL53L0X SCL | I²C clock (downward ToF) |
| P0.03 | VL53L0X SDA | I²C data (downward ToF) |
| P0.04 | ICM-42688 SCL | I²C clock (IMU) |
| P0.05 | ICM-42688 SDA | I²C data (IMU) |
| P0.06 | DRV2605L SCL | I²C clock (haptic) |
| P0.07 | DRV2605L SDA | I²C data (haptic) |
| P0.08 | HC-SR04 TRIG | Ultrasonic trigger output |
| P0.09 | HC-SR04 ECHO | Ultrasonic echo input (interrupt) |
| P0.10 | VL53L0X INT | ToF interrupt (ready signal) |
| P0.11 | Battery voltage | ADC (AIN11) |
| P0.12 | USB power detect | Input only |
| P0.13 | Status LED | Output |
| P0.14 | Tap detect (IMU INT) | IMU interrupt (wake on motion) |
| P0.15 | Motor enable | DRV2605L EN |

### 3.4 Haptic Band

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, BLE 5.0 |
| IMU | ICM-42688 | Fall detection (200 Hz accel), arm gesture, step counting |
| Haptic Driver | DRV2605L | Navigation haptics + alert patterns |
| Haptic Motor | ERM motor 10mm | Wrist vibration: navigation (left/right/stop/arrive), alerts (fall, SOS) |
| SOS Button | Tactile button | Large, textured, long-press 3s → emergency dispatch |
| BLE | Built-in BLE 5.0 | Peripheral to Hub |
| Battery | LiPo 3.7V 300 mAh | ~48 hours operation (low-power BLE + duty-cycled IMU) |
| Charger | MCP73871 | USB-C charging |
| Enclosure | Silicone wristband | IP67, hypoallergenic, <25 g |
| LED | Single LED | Status + SOS indicator |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | ICM-42688 SCL | I²C clock (IMU) |
| P0.03 | ICM-42688 SDA | I²C data (IMU) |
| P0.04 | DRV2605L SCL | I²C clock (haptic) |
| P0.05 | DRV2605L SDA | I²C data (haptic) |
| P0.06 | SOS button | Input with pull-up, external interrupt |
| P0.07 | IMU INT1 | Accelerometer interrupt (data ready / free-fall) |
| P0.08 | IMU INT2 | Gyro interrupt (orientation change) |
| P0.09 | Battery voltage | ADC (AIN9) |
| P0.10 | Motor enable | DRV2605L EN |
| P0.11 | Status LED | Output |
| P0.12 | USB power detect | Input only |

### 3.5 Nav Beacon (×N, up to 32)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | nRF52840 QFAA | Cortex-M4F 64 MHz, BLE 5.0, ultra-low power |
| BLE | Built-in BLE 5.0 | Advertising mode only (iBeacon/Eddystone compatible) |
| Battery | CR2032 coin cell | 12–18 months (BLE advertising every 500 ms) |
| Landmark ID | Programmed via BLE | Each beacon has unique UUID + landmark metadata (e.g., "kitchen door", "elevator", "restroom") |
| Enclosure | 3D-printed ASA | Wall-mounted, 35 mm diameter, 12 mm thick, adhesive backing |
| LED | Single LED | Beacon active / battery low indicator |
| Switch | Reed switch | Magnetic wake/program switch (configuration mode) |

**Pin Assignments (nRF52840):**

| GPIO | Function | Notes |
|------|----------|-------|
| P0.02 | Status LED | Output |
| P0.03 | Reed switch | Input with pull-up (config mode) |
| P0.04 | Battery voltage | ADC (AIN4) — periodic battery check |

---

## 4. Communication Protocol

### 4.1 Physical & Link Layer

- **Band:** 2.4 GHz BLE 5.0 (all nodes)
- **Topology:** Star network — Vision Hub as central, glasses/cane/band as peripherals; Nav Beacons as BLE advertisers (broadcast-only, no connection required)
- **Connection interval:** 50 ms (glasses, high data), 200 ms (cane, band), 500 ms advertising (beacons)
- **Encryption:** AES-128-CCM (BLE built-in LE Secure Connection, per-node LTK)
- **Range:** 10 m indoor (BLE 5.0), 30 m LOS — Hub pocket-carried stays within range of all wearables
- **Max nodes:** 3 peripherals (glasses, cane, band) + 32 beacons (advertising) = 35 nodes
- **BLE services:** Custom GATT service `0xGS01` (GuideSync) with characteristics for telemetry, commands, nav data, alerts

### 4.2 Message Format

All BLE GATT notifications use a compact binary protocol (8–256 bytes):

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │
│ 0x47 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┘
```

Sync bytes: `0x47 0x53` = "GS" (GuideSync). BLE link-layer provides CRC and encryption, so no application-layer CRC is needed (unlike the Sub-GHz systems).

### 4.3 Message Types

| Type | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | JOIN_REQ | Node→Hub | Capabilities, battery, FW version |
| 0x02 | JOIN_ACK | Hub→Node | NodeID, connection params, beacon map hash |
| 0x03 | TELEMETRY | Node→Hub | Node-specific telemetry (see below) |
| 0x04 | COMMAND | Hub→Node | Command sub-type + params |
| 0x05 | CMD_ACK | Node→Hub | Command acknowledgment |
| 0x06 | ALERT | Node→Hub | Alert type + severity + data |
| 0x07 | OTA_BLOCK | Hub→Node | Firmware chunk |
| 0x08 | OTA_ACK | Node→Hub | Chunk received + CRC |
| 0x09 | HEARTBEAT | Node→Hub | Battery, RSSI, uptime |
| 0x0A | NAV_UPDATE | Hub→Band | Navigation instruction (direction, distance, landmark) |
| 0x0B | SCENE_DESC | Glasses→Hub | Scene description (objects, distances, text) |
| 0x0C | OCR_REQUEST | Glasses→Hub | Image chunk for cloud/hub OCR |
| 0x0D | OCR_RESULT | Hub→Glasses | Recognized text string |
| 0x0E | FALL_ALERT | Band→Hub | Fall detected + impact magnitude + GPS request |
| 0x0F | SOS_ALERT | Band→Hub | SOS triggered + GPS request |
| 0x10 | BEACON_SCAN | Glasses/Band→Hub | BLE beacon RSSI fingerprint (for NavNet) |
| 0x11 | NAV_DEST | Hub→Glasses | Navigation destination + route steps |
| 0x12 | CALIBRATION | Hub→Node | Calibration parameters |
| 0x13 | CALIB_ACK | Node→Hub | Calibration result |
| 0x14 | TIME_SYNC | Hub→All | Epoch timestamp |

### 4.4 Telemetry Payloads

**Glasses telemetry (28 bytes):**
- Battery voltage (1), head_pitch (1), head_roll (1), head_yaw (1)
- Nearest obstacle class (1), nearest_obstacle_distance_dm (1), nearest_obstacle_direction (1)
- Scene object count (1), primary_object_class (1), primary_object_distance_dm (1)
- Crosswalk detected (1), signal_state (1: 0=none, 1=walk, 2=don't_walk, 3=countdown), countdown_sec (1)
- ToF grid min_distance_dm (1), ToF grid hazard_flag (1)
- Audio_volume (1), bone_conduction_active (1)
- Step_count_24h (2), IMU_temp (1)
- SceneNet_inference_ms (2), CrosswalkNet_inference_ms (2)
- Free_heap (2), BLE_rssi (1), uptime_min (2)
- ToF_valid_zones (1), reserved (1)

**Cane telemetry (14 bytes):**
- Battery voltage (1), ultrasonic_distance_dm (1), ultrasonic_valid (1)
- ToF_downward_distance_dm (1), dropoff_detected (1), stair_detected (1)
- IMU_swing_count_24h (2), IMU_temp (1)
- Haptic_pattern_last (1), haptic_active (1)
- Cane_tilt_deg (1), step_count_24h (2)
- BLE_rssi (1)

**Haptic band telemetry (12 bytes):**
- Battery voltage (1), IMU_temp (1)
- Step_count_24h (2), fall_count_24h (1)
- Haptic_pattern_last (1), nav_direction (1), nav_distance_m (1)
- SOS_armed (1), BLE_rssi (1)
- Uptime_min (2)

**Nav beacon scan (variable, 4 + 8×N_beacons):**
- Beacon count (1), reserved (3)
- Per beacon: UUID_short (2), RSSI (1), distance_est_dm (1) × N

### 4.5 Navigation Haptic Patterns

The haptic band uses DRV2605L waveform sequences for navigation:

| Instruction | Pattern | DRV2605L Sequence |
|-------------|---------|-------------------|
| Continue straight | Single short pulse every 5s | [Sharp click, 60ms] |
| Turn left | Double pulse, repeat 3× | [Strong click, 80ms, 50ms gap, strong click, 80ms] |
| Turn right | Triple pulse, repeat 3× | [Sharp click ×3, 60ms each] |
| Stop / obstacle ahead | Long continuous vibration | [Long hum, 500ms] |
| Arrived at destination | Ascending triple pulse | [Soft bump, medium click, strong click] |
| Fall detected | Urgent alternating pattern | [Strong rumble 200ms ×5, 100ms gaps] |
| SOS confirmed | Descending triple pulse | [Strong click, medium click, soft bump] |
| Low battery (band) | Single weak pulse every 60s | [Soft bump, 40ms] |

---

## 5. Firmware Architecture

### 5.1 Common Code

All nodes share a common codebase in `firmware/common/`:
- `config.h` — Pin assignments, BLE UUIDs, network parameters, thresholds
- `protocol.h` / `protocol.c` — Binary message encoding/decoding (shared format)
- `ble_mesh.h` / `ble_mesh.c` — BLE 5.0 star network layer (Hub = central, nodes = peripherals)
- `ble_beacon.h` / `ble_beacon.c` — BLE beacon scanning (RSSI fingerprinting for NavNet)

### 5.2 Per-Node Firmware

| Node | MCU | RTOS | Key Functions |
|------|-----|------|---------------|
| Vision Hub | ESP32-S3 | FreeRTOS | BLE central, Wi-Fi/MQTT bridge, 4G LTE backup, OCR relay, OTA distribution, emergency dispatch |
| Smart Glasses | ESP32-S3 | FreeRTOS | Camera capture, SceneNet YOLOv8-nano inference, VL53L5CX ToF read, ObstacleNet, CrosswalkNet, bone conduction audio output, I²S voice command, ICM-42688 head tracking |
| Smart Cane | nRF52840 | Zephyr RTOS | HC-SR04 ultrasonic, VL53L0X downward ToF, ICM-42688 swing tracking, DRV2605L handle haptics |
| Haptic Band | nRF52840 | Zephyr RTOS | ICM-42688 fall detection (FallNet 1D-CNN), DRV2605L nav haptics, SOS button, BLE beacon scan |
| Nav Beacon | nRF52840 | Bare-metal (nRF SDK) | BLE advertising, CR2032 power management, reed switch config mode |

---

## 6. ML Pipeline (6 Models)

### 6.1 SceneNet — Real-Time Object Detection (YOLOv8-nano)

**Objective:** Detect and classify objects, obstacles, and hazards in the user's path from the glasses camera.

**Architecture:** YOLOv8-nano (nano variant for ESP32-S3)
- Backbone: CSPDarknet-tiny (6.6M params)
- Neck: PANet-tiny FPN
- Head: Decoupled detection head (anchor-free)
- Input: 320×320 RGB (downsampled from OV5640 2592×1944)
- Output: Bounding boxes + class labels + confidence (80 classes)
- Post-processing: NMS (IoU 0.45, conf 0.35)

**Classes (80, COCO subset prioritized for mobility):**
Priority classes for blind navigation: person, chair, table, bed, couch, toilet, door, stairs, elevator, escalator, bicycle, car, motorcycle, bus, truck, traffic light, stop sign, backpack, handbag, suitcase, bottle, cup, fork, knife, bowl, banana, apple, laptop, cell phone, keyboard, book, clock, vase, scissors, teddy bear, dog, cat, bird, plus custom: white_cane, guide_dog, mobility_scooter, trash_can, pole, wall, doorway, curb, puddle, overhanging_branch

**Training:** COCO 2017 (118K images) + custom blind-navigation dataset (5,000 annotated images from head-level perspective, including low-light, indoor, outdoor)
**Data augmentation:** mosaic, mixup, horizontal flip, HSV jitter, low-light simulation
**Metrics:** 37.2 mAP@0.5 (COCO), 89.1% recall on priority mobility classes, 72 fps on ESP32-S3 (320×320, int8)
**Edge deployment:** TFLite-Micro int8 quantized (~3.8 MB) runs on ESP32-S3 in <300 ms per frame (duty-cycled: 2 fps continuous, 5 fps when obstacle detected)

### 6.2 ObstacleNet — ToF Depth Grid Hazard Classifier

**Objective:** Classify the 8×8 VL53L5CX ToF depth grid into hazard categories and determine proximity.

**Architecture:** 2-layer 1D-CNN over 64-zone depth vector
- Input: 64×1 depth values (dm), normalized
- Conv1D(32, k=3) + ReLU + MaxPool1D(2) → Conv1D(16, k=3) + ReLU + MaxPool1D(2)
- Flatten → Dense(32) + ReLU → Dense(6) + Softmax

**Classes (6):**
| # | Class | Description | Action |
|---|-------|-------------|--------|
| 0 | Clear | No obstacle within 2 m | Continue |
| 1 | Obstacle_low | Object 0–1 m, lower zones | "Obstacle ahead, step over" |
| 2 | Obstacle_high | Object 0–1 m, upper zones | "Obstacle at head level" |
| 3 | Obstacle_side | Object 0–1 m, left/right zones | "Obstacle left/right" |
| 4 | Approaching | Object 1–2 m, closing | "Approaching obstacle" |
| 5 | Open_space | All zones >2 m | "Clear path ahead" |

**Training:** 20,000 synthetic ToF grids (ray-traced from 3D indoor models) + 5,000 real VL53L5CX recordings
**Metrics:** 93.7% accuracy, 96.2% recall on hazard classes (0–4)
**Edge deployment:** TFLite-Micro int8 (~80 KB), inference <20 ms on ESP32-S3

### 6.3 TextReader — OCR for Signs, Labels & Menus

**Objective:** Read text from the glasses camera (signs, menus, product labels, medication instructions) and speak it via bone conduction.

**Architecture:** Two-stage pipeline (runs on Hub for heavier inference):
1. **Text detection:** EAST (Efficient and Accurate Scene Text) detector — 32×32 feature map, quadrilateral text boxes
2. **Text recognition:** CRNN (CNN + BiLSTM + CTC) — character sequence recognition, 97-character alphabet (a-z, A-Z, 0-9, punctuation, common symbols)

**Flow:**
- User says "read text" → glasses captures image → sends to Hub via BLE (compressed, chunked) → Hub runs EAST + CRNN → returns text string → glasses speaks via bone conduction (TTS)
- For long text (menus, documents): Hub paginates, user says "next" to continue

**Training:** ICDAR 2015 (scene text) + COCO-Text + custom medication label dataset (1,000 labeled pharmacy labels)
**Metrics:** 84.2% end-to-end accuracy (scene text), 91.5% on printed labels (high contrast), 16 chars/sec
**Edge deployment:** EAST + CRNN ONNX models (~12 MB total) on Hub ESP32-S3 (PSRAM), inference ~1.2s per image

### 6.4 NavNet — Indoor Positioning from BLE Beacon Fingerprinting

**Objective:** Determine the user's indoor position from BLE beacon RSSI fingerprints and provide turn-by-turn navigation.

**Architecture:** 2-layer LSTM (64 hidden units) → Dense(2) (x, y position)
- Input: RSSI values from up to 8 nearest beacons + beacon UUIDs (one-hot) + IMU heading + step count delta
- Output: 2D position (x, y) in building coordinate space (meters)
- Temporal window: 10 seconds of RSSI history (20 samples at 2 Hz)

**Training:** 50,000 RSSI fingerprints collected by walking 12 buildings with mapped beacon placements (fingerprinting + dead reckoning fusion)
**Metrics:** ±1.2 m median positioning error (beaconed areas), ±3.8 m in sparse-beacon areas
**Route planning:** A* on building graph (nodes = beacons/landmarks, edges = walkable paths)
**Turn-by-turn:** At each decision point, Hub sends NAV_UPDATE to haptic band with direction + distance to next waypoint

**Beacon map:** Cloud maintains a registry of all user-deployed beacons (UUID, landmark name, x/y coordinates, floor). User or caregiver maps beacons via mobile app (walk to beacon, press "mark location").

### 6.5 CrosswalkNet — Crosswalk & Pedestrian Signal Detection

**Objective:** Detect crosswalks, pedestrian signals (walk/don't-walk), and countdown timers from the glasses camera.

**Architecture:** MobileNetV3-small + detection head
- Input: 224×224 RGB (cropped lower half of camera frame where signals appear)
- Backbone: MobileNetV3-small (2.5M params)
- Head: Classification (3-class: no_crosswalk, crosswalk, signal_visible) + signal_state (4-class: none, walk, don't_walk, countdown)

**Classes:**
| # | Signal State | Action |
|---|-------------|--------|
| 0 | None | No crosswalk detected |
| 1 | Walk (white person) | "Crosswalk ahead, walk signal on" |
| 2 | Don't walk (red hand) | "Crosswalk ahead, wait — don't walk signal" |
| 3 | Countdown | "Crosswalk, X seconds remaining" (reads countdown number) |

**Training:** 8,000 labeled crosswalk images (day/night, 6 countries, varied signal designs) + augmentation (blur, rain, glare)
**Metrics:** 91.4% accuracy, 94.8% recall on walk/don't-walk states, <50 ms false negative tolerance (safety-critical)
**Edge deployment:** TFLite-Micro int8 (~220 KB), inference <80 ms on ESP32-S3

### 6.6 FallNet — Fall Detection & Sensor Anomaly

**Objective:** Detect falls from the haptic band IMU (200 Hz accelerometer) and identify sensor anomalies across all nodes.

**Architecture (Fall Detection):** 1D-CNN over 2-second accel window (400 samples × 3 axes)
- Conv1D(32, k=7) + ReLU + MaxPool1D(2) → Conv1D(16, k=5) + ReLU + MaxPool1D(2)
- Flatten → Dense(32) + ReLU + Dropout(0.2) → Dense(3) + Softmax

**Classes (3):**
| # | Class | Description |
|---|-------|-------------|
| 0 | Normal | Standing, walking, sitting, cane use |
| 1 | Fall | Free-fall + impact + post-fall stillness |
| 2 | Activity | Stumbling, bending, quick movements (not falls) |

**Training:** SisFall dataset (4,505 fall + activity recordings, 36 subjects) + UMAFall + custom blind-user cane recordings
**Metrics:** 96.1% sensitivity, 0.21 FP/day, <400 ms inference on nRF52840
**Edge deployment:** TFLite-Micro int8 (~45 KB) on nRF52840

**Sensor Anomaly (Isolation Forest):**
- Input: All telemetry fields across all nodes (18-dim feature vector)
- Detects: camera obscured/blocked, ToF fogged, ultrasonic blocked, IMU stuck, battery drain, BLE dropout patterns
- 100 trees, 256 sample size, trained on 6 months normal operation data

---

## 7. Cloud Backend

### 7.1 Architecture

```
                    ┌─────────────┐
                    │  Mobile App │  (caregiver + user with VoiceOver)
                    └──────┬──────┘
                           │ HTTPS (REST + WebSocket)
                    ┌──────▼──────┐
                    │   FastAPI    │
                    │  (Uvicorn)   │
                    └──────┬──────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
    ┌──────▼──────┐ ┌──────▼──────┐ ┌──────▼──────┐
    │ PostgreSQL  │ │  InfluxDB   │ │  MQTT Broker │
    │ (devices,   │ │ (telemetry │ │  (mosquitto) │
    │  users,     │ │  time-series│ │              │
    │  beacons,   │ │  data)     │ │              │
    │  faces)     │ │             │ │              │
    └─────────────┘ └─────────────┘ └─────────────┘
                           │
                    ┌──────▼──────┐
                    │ ML Pipeline  │
                    │ (PyTorch +   │
                    │  ONNX runtime│
                    │  + Celery    │
                    │  workers)    │
                    └─────────────┘
```

### 7.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/v1/auth/login` | User login (JWT) |
| GET | `/api/v1/devices` | List all devices |
| POST | `/api/v1/devices/{id}/ota` | Trigger OTA update |
| GET | `/api/v1/glasses` | Latest glasses telemetry |
| GET | `/api/v1/glasses/scene` | Latest scene description |
| GET | `/api/v1/cane` | Latest cane telemetry |
| GET | `/api/v1/band` | Latest haptic band telemetry |
| GET | `/api/v1/beacons` | List all nav beacons |
| POST | `/api/v1/beacons` | Register new beacon |
| PUT | `/api/v1/beacons/{id}` | Update beacon (landmark, position) |
| GET | `/api/v1/navigation/route` | Get route from A to B |
| POST | `/api/v1/navigation/destination` | Set navigation destination |
| GET | `/api/v1/navigation/status` | Current nav status |
| POST | `/api/v1/ocr/request` | Request OCR on uploaded image |
| GET | `/api/v1/alerts` | List alerts |
| PUT | `/api/v1/alerts/{id}/ack` | Acknowledge alert |
| POST | `/api/v1/sos/cancel` | Cancel SOS (false alarm) |
| GET | `/api/v1/emergency/contacts` | List emergency contacts |
| POST | `/api/v1/emergency/contacts` | Add emergency contact |
| GET | `/api/v1/faces` | List familiar faces (encrypted) |
| POST | `/api/v1/faces` | Add familiar face (encrypted embedding) |
| GET | `/api/v1/ml/scene/history` | Scene detection history |
| GET | `/api/v1/ml/nav/position` | Latest NavNet position |
| GET | `/api/v1/ml/fall/history` | Fall event history |
| GET | `/api/v1/location` | Current GPS + indoor position |
| WS | `/api/v1/ws` | Real-time WebSocket (telemetry, alerts, scene) |

### 7.3 MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `guidesync/{user}/hub/telemetry` | Hub→Cloud | Aggregated telemetry JSON |
| `guidesync/{user}/hub/scene` | Hub→Cloud | Scene description events |
| `guidesync/{user}/hub/cane` | Hub→Cloud | Cane telemetry |
| `guidesync/{user}/hub/band` | Hub→Cloud | Haptic band telemetry |
| `guidesync/{user}/hub/fall` | Hub→Cloud | Fall alert (critical priority) |
| `guidesync/{user}/hub/sos` | Hub→Cloud | SOS alert (critical priority) |
| `guidesync/{user}/hub/beacon_scan` | Hub→Cloud | BLE beacon RSSI fingerprints |
| `guidesync/{user}/cloud/command` | Cloud→Hub | Navigation destination, config |
| `guidesync/{user}/cloud/ota` | Cloud→Hub | OTA firmware blocks |
| `guidesync/{user}/cloud/alert` | Cloud→Hub | Alert notifications |

---

## 8. Mobile App (React Native)

### Screens

1. **Dashboard** — System status (glasses, cane, band, hub online/offline), battery levels, current location (GPS + indoor), active navigation, recent alerts, SOS status
2. **Scene View** — Latest scene description (objects detected, distances, directions), scene detection history timeline, "last seen" object log
3. **Navigation** — Set destination, current route, turn-by-turn steps, beacon map view, indoor position marker, estimated arrival time
4. **Beacons** — List of all registered beacons, add/register new beacon (walk to beacon, scan BLE, set landmark name + coordinates), edit beacon positions on floor map
5. **Reading** — OCR history (recent text read), re-read button, "read from camera" live mode, document pagination
6. **Alerts** — Active and historical alerts (fall detected, SOS, low battery, obstacle collision, sensor anomaly, beacon offline)
7. **Emergency** — Emergency contacts list, add/edit contacts, SOS test, fall detection sensitivity slider, auto-911 toggle, caregiver location sharing
8. **Faces** — Familiar face database (encrypted), add new face (name + photo), face recognition toggle, privacy controls
9. **Caregiver** — (For sighted caregivers) Live location tracking, navigation history, fall/SOS alert log, remote beacon mapping, device health
10. **Settings** — Device management, calibration, audio volume (bone conduction), haptic intensity, voice command language, notification preferences, privacy settings

### Features
- VoiceOver/TalkBack fully compatible (app is usable by blind users)
- Push notifications (fall detected, SOS triggered, low battery, arrival at destination)
- Offline caching of last-known data and beacon maps
- Caregiver sharing (real-time location + alerts)
- Beacon mapping wizard (sighted caregiver walks building, places beacons, marks floor plan)
- Familiar face enrollment (privacy-controlled, encrypted embeddings, on-device matching)
- Custom landmark recording (user records audio tag for beacon: "this is the kitchen door")
- Voice command configuration (custom phrases for "read text", "describe scene", "where am I")

---

## 9. Power Architecture

| Node | Power Source | Battery | Avg Consumption | Autonomy |
|------|-------------|---------|-----------------|----------|
| Vision Hub | USB-C / LiPo | 2000 mAh | ~120 mA @ 3.7V (portable) | 12 hours portable, continuous on USB |
| Smart Glasses | LiPo | 800 mAh | ~110 mA @ 3.7V (camera + CNN + bone conduction) | 6 hours continuous |
| Smart Cane | LiPo | 500 mAh | ~12 mA avg (duty-cycled) | 20+ hours |
| Haptic Band | LiPo | 300 mAh | ~3 mA avg (BLE + duty-cycled IMU) | 48+ hours |
| Nav Beacon | CR2032 | 220 mAh | ~0.03 mA avg (500 ms advertising) | 12–18 months |

### Critical Power Design

The **Smart Glasses** are the most power-hungry node (camera + CNN + bone conduction). Design choices:
- **Duty-cycling:** Camera captures 2 fps continuously, 5 fps when obstacle detected (SceneNet triggers higher rate)
- **PSRAM for model:** SceneNet int8 model lives in PSRAM (loaded once at boot)
- **Sleep between frames:** ESP32-S3 light-sleep between camera frames (saves ~40% power)
- **Bone conduction low-power:** Audio amp enabled only during speech output
- **6-hour target:** Covers a full day out with a midday charge (USB-C power bank)

The **Haptic Band** runs FallNet continuously (200 Hz IMU) — nRF52840's ultra-low-power mode with IMU interrupt wake achieves 48+ hours on 300 mAh.

---

## 10. Safety & Reliability

### Fall Detection & Emergency Protocol
1. Haptic band IMU detects free-fall (accel <0.5g for >50 ms) + impact (>2.5g) + post-fall stillness
2. FallNet 1D-CNN confirms fall (96% sensitivity, <0.3 FP/day)
3. Band sends FALL_ALERT to Hub via BLE with impact magnitude
4. Hub immediately:
   - Sends SMS to all emergency contacts with GPS location
   - Calls 911 via 4G LTE (SIM7000) with automated voice message + GPS coordinates
   - Activates haptic band urgent vibration pattern
   - Publishes fall alert to cloud (caregiver app notification)
5. User has 30-second cancel window (press SOS button 2× to cancel false alarm)
6. If not cancelled, emergency dispatch proceeds

### SOS Protocol
1. User long-presses SOS button on haptic band (3 seconds)
2. Band sends SOS_ALERT to Hub
3. Hub sends SMS + calls emergency contacts + 911
4. Haptic band confirms SOS with descending vibration pattern
5. User can cancel within 60 seconds (press SOS 3× rapidly)

### Obstacle Safety Interlocks
1. **SceneNet + ObstacleNet dual-confirmation:** Audio alert only fires when both camera CNN and ToF array agree on obstacle proximity (<1 m)
2. **Cane + glasses coverage overlap:** Cane detects ground-level (0–1 m height), glasses detect head/torso level (1–2 m height)
3. **Crosswalk safety:** CrosswalkNet must detect walk signal with >90% confidence before announcing "walk signal on"
4. **Audio priority:** Safety-critical alerts (obstacle, crosswalk, fall) preempt non-critical audio (scene description, text reading)
5. **Open-ear design:** Bone conduction preserves environmental hearing — user always hears traffic, people, and ambient cues

### Data Reliability
- SD card buffering on Hub (14-day capacity at full telemetry rate)
- 4G LTE cellular backup for emergency alerts during Wi-Fi outage
- BLE star with automatic reconnection (peripheral reconnects within 2 s of dropout)
- OTA firmware updates with rollback (dual-partition on ESP32-S3, A/B on nRF52840)
- BLE LE Secure Connection encryption (AES-128-CCM)
- Cloud data encrypted at rest (PostgreSQL TDE, InfluxDB encryption)

### Privacy
- **On-device processing:** SceneNet, ObstacleNet, CrosswalkNet, FallNet all run on-device — no video/audio leaves the glasses unless user requests OCR
- **OCR opt-in:** TextReader only activates on explicit voice command ("read text")
- **Face recognition opt-in:** Familiar face matching is disabled by default, encrypted embeddings stored on-device
- **No cloud video:** Camera frames are never streamed to cloud; only scene descriptions (text labels) are sent
- **BLE beacon privacy:** Beacons broadcast only UUID + RSSI, no personal data

---

## 11. Bill of Materials

See `hardware/bom/` for per-node BOM CSV files.

### System Cost Estimate (1 hub + glasses + cane + band + 8 beacons)

| Node | Qty | Unit Cost | Total |
|------|-----|-----------|-------|
| Vision Hub | 1 | $72.40 | $72.40 |
| Smart Glasses | 1 | $118.50 | $118.50 |
| Smart Cane | 1 | $64.20 | $64.20 |
| Haptic Band | 1 | $38.70 | $38.70 |
| Nav Beacon | 8 | $11.80 | $94.40 |
| **Total** | | | **$388.20** |

---

## 12. Accessibility & Social Impact

- **2.2B people with vision impairment** — GuideSync provides independent navigation, reading, and safety for the world's largest underserved disability population
- **Employment enablement** — 70% of blind working-age adults are unemployed; GuideSync's reading + navigation capabilities enable greater workplace independence
- **Fall prevention** — Blind adults have 2× the fall rate of sighted adults; FallNet + SOS reduces time-to-help from hours to minutes
- **Indoor navigation** — First consumer system with BLE beacon-based indoor positioning (GPS doesn't work indoors, where blind users need the most help)
- **Privacy-first** — On-device AI means no surveillance; blind users control when the camera is used for reading
- **Open-source** — MIT licensed; organizations can deploy at scale without licensing fees
- **Low cost** — $388 system cost vs. $3,500+ for OrCam MyEye or $2,000+ for Envision Glasses
- **Modular** — Users can start with just glasses + hub, add cane/beacons/band as needed

---

## 13. File Structure

```
GuideSync/
├── README.md                    # This file
├── schematic/
│   ├── README.md                 # Schematic overview
│   ├── hub/                      # Vision Hub schematic (KiCad)
│   ├── smart-glasses/            # Smart Glasses schematic
│   ├── smart-cane/               # Smart Cane schematic
│   ├── haptic-band/              # Haptic Band schematic
│   └── nav-beacon/               # Nav Beacon schematic
├── firmware/
│   ├── common/                   # Shared protocol, BLE, config code
│   ├── hub/                      # Vision Hub firmware (ESP32-S3, FreeRTOS)
│   ├── smart-glasses/            # Smart Glasses firmware (ESP32-S3)
│   ├── smart-cane/               # Smart Cane firmware (nRF52840, Zephyr)
│   ├── haptic-band/              # Haptic Band firmware (nRF52840, Zephyr)
│   └── nav-beacon/               # Nav Beacon firmware (nRF52840)
├── hardware/
│   └── bom/                      # BOM CSVs per node
├── software/
│   ├── dashboard/                # FastAPI backend
│   ├── ml-pipeline/              # ML training + inference scripts
│   └── mobile-app/               # React Native app
├── docs/
│   ├── architecture.md
│   ├── api-spec.md
│   └── protocol-spec.md
└── scripts/
    ├── deploy.sh                 # Cloud deployment
    ├── calibrate_sensors.py      # Sensor calibration
    └── train_models.py           # ML training pipeline runner
```

---

## License

MIT — build it, sell it, improve it.

---

*Invented as part of the [Devices](https://github.com/jayis1/Devices) project — complex hardware+software systems that improve daily life.*