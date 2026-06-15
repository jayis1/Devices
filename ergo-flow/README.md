# ErgoFlow

**AI-powered adaptive workspace wellness system.** Prevents back pain, eye strain, RSI, and burnout for the 1 billion+ desk workers worldwide — by continuously monitoring your posture, gaze, movement patterns, and environment, then autonomously adjusting your workspace to keep you healthy and focused.

---

## What It Does

ErgoFlow is a 5-node system that transforms any desk into an intelligent wellness workspace:

1. **Monitors** posture continuously via pressure mapping (chair pad), IMU motion tracking (wearable tag), and mmWave radar (hub camera alternative — privacy-first)
2. **Detects** early signs of RSI, eye strain, and musculoskeletal stress using ML models trained on biomechanical data
3. **Adjusts** desk height, monitor angle, and ambient lighting autonomously to optimize ergonomics
4. **Nudges** you to take micro-breaks, stretch, and hydrate — timed to your natural focus cycles (ultradian rhythms)
5. **Adapts** lighting color temperature and intensity throughout the day to support circadian health (blue-enriched morning, warm evening)
6. **Learns** your personal posture patterns, focus rhythms, and injury risk profile over time
7. **Reports** long-term trends to your phone: posture score, break compliance, focus analytics, risk predictions

All nodes communicate over BLE mesh for low-power operation, with the hub bridging to WiFi/cloud. The system works offline for all critical posture monitoring — cloud is only for analytics and ML model updates.

### The Problem It Solves

- **1.4 billion** desk workers worldwide suffer from back pain, neck strain, or RSI
- Musculoskeletal disorders are the #1 workplace disability cause globally
- 60% of desk workers develop chronic neck pain within 5 years
- Eye strain (digital eye fatigue) affects 65–90% of screen users
- The average desk worker sits for 11+ hours/day, taking only 2–3 breaks
- Existing "solutions" are static: standing desks you forget to adjust, reminder apps you dismiss, ergonomic chairs you slouch in anyway
- **ErgoFlow makes the workspace respond to YOU** — not the other way around

---

## System Architecture

```
┌───────────────────────────────────────────────────────────────────────────┐
│                        ERGOFLOW SYSTEM                                    │
│                                                                           │
│  ┌──────────────────┐              ┌──────────────────┐                   │
│  │   CHAIR PAD      │   BLE mesh   │                  │                   │
│  │   (under seat    │◄────────────►│                  │                   │
│  │    + back)        │              │                  │                   │
│  │   Pressure array │              │                  │                   │
│  │   16× FSR grid   │              │    HUB NODE      │                   │
│  │   IMU (posture)  │              │  (nRF5340 +       │──── WiFi6 ───► Cloud
│  └──────────────────┘              │   ESP32-C6)       │                  Dashboard
│                                    │                  │                  + ML
│  ┌──────────────────┐              │  mmWave radar    │                  Pipeline
│  │  DESK CONTROLLER │◄────────────►│  (pose detection) │                 + Alerts
│  │  (on desk leg)   │  BLE mesh   │  Ambient sensors  │
│  │  Linear actuator │              │  RGBW LED ctrl    │─── BLE ──────► Mobile App
│  │  Monitor tilt    │              │  Speaker/mic      │                  (React Native)
│  │  Ambient light   │              │  nFC + UI button  │
│  └──────────────────┘              └──────┬───────────┘
│                                            │ BLE mesh
│  ┌──────────────────┐                     │
│  │  WEARABLE TAG    │◄────────────────────┘
│  │  (wrist clip or  │   (up to 2 tags per hub:
││   lanyard)        │    primary user +
│  │  IMU 9-DOF      │    guest)
│  │  vib motor      │
│  │  pulse ox (opt) │
│  └──────────────────┘
│
│  ┌──────────────────────────────────────────────────────────────────────┐
│  │                    CLOUD / EDGE SOFTWARE                            │
│  │  ┌─────────────┐  ┌────────────────┐  ┌────────────────────────┐   │
│  │  │  Dashboard  │  │  ML Pipeline   │  │  Mobile App            │   │
│  │  │  (FastAPI + │  │  (PyTorch)     │  │  (React Native)        │   │
│  │  │   React)    │  │  Posture CNN   │  │  Posture score live    │   │
│  │  │  Realtime   │  │  RSI risk LSTM │  │  Break reminders       │   │
│  │  │  History    │  │  Focus detector│  │  Stretch guides (AR)   │   │
│  │  │  Trends     │  │  Gaze tracker  │  │  Desk control widget   │   │
│  │  │  Config     │  │  Circadian ML  │  │  Health report weekly  │   │
│  │  └─────────────┘  └────────────────┘  └────────────────────────┘   │
│  └──────────────────────────────────────────────────────────────────────┘
└───────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per workstation)

The brain. Sits on your desk or mounts under the monitor. Bridges BLE mesh to WiFi/cloud. Runs local inference for real-time posture detection.

| Component | Part | Purpose |
|-----------|------|---------|
| Primary MCU | nRF5340 | BLE mesh coordinator + sensor fusion + local ML inference |
| WiFi Co-MCU | ESP32-C6-MINI-1 | WiFi6 uplink to MQTT broker, OTA updates |
| mmWave Radar | Infineon BGT60TR13C | 60GHz FMCW radar for pose/proximity detection (privacy-first — no camera images) |
| Ambient Light | TSL2591 (I2C) | Lux + IR for circadian lighting control |
| Temp/Humidity | SHT40 (I2C) | Environmental comfort monitoring |
| RGBW LED Driver | LP5562 (I2C) | Drives desk ambient lighting strip (4 channels) |
| Audio | MEMS mic (SPH0645) + MAX98357A amp | Break reminders, voice status, ambient sound detection |
| Display | 1.3" OLED (SH1106, I2C) | Status: posture score, break timer, connection state |
| Haptic | DRV2605L (I2C) + ERM | Desk-level vibration alerts |
| Storage | 16MB QSPI Flash + microSD | Data logging, OTA cache, local model weights |
| Power | 5V USB-C + 2000mAh LiPo | Runs ~8h on battery during power outage |
| Button | 4× tactile buttons | UI: dismiss break, force sit/stand, mute, pair |
| Connectors | Qwiic/StemmaQT (I2C), USB-C, 4× GPIO | Expansion, debug, power |

**Pin Assignments (nRF5340):**

| Pin | Function | Notes |
|-----|----------|-------|
| P0.00 | I2C SDA | TSL2591, SHT40, LP5562, SH1106, DRV2605L |
| P0.01 | I2C SCL | Shared I2C bus @ 400kHz |
| P0.02 | SPI CLK | mmWave radar SPI bus |
| P0.03 | SPI MOSI | mmWave radar SPI |
| P0.04 | SPI MISO | mmWave radar SPI |
| P0.05 | SPI CS (mmWave) | BGT60TR13C chip select |
| P0.06 | mmWave IRQ | Data-ready interrupt from radar |
| P0.07 | UART TX → ESP32-C6 | Hub ↔ WiFi co-processor serial |
| P0.08 | UART RX ← ESP32-C6 | Hub ↔ WiFi co-processor serial |
| P0.09 | ESP32 BOOT | Boot mode for ESP32-C6 |
| P0.10 | ESP32 RESET | Reset line for ESP32-C6 |
| P0.11 | SD SPI CLK | microSD card SPI |
| P0.12 | SD SPI MOSI | microSD card SPI |
| P0.13 | SD SPI MISO | microSD card SPI |
| P0.14 | SD SPI CS | microSD card SPI |
| P0.15 | MIC I2S CLK | SPH0645 I2S clock |
| P0.16 | MIC I2S DOUT | SPH0645 I2S data |
| P0.17 | AMP I2S DIN | MAX98357A I2S data in |
| P0.18 | AMP I2S BCLK | MAX98357A I2S bit clock |
| P0.19 | AMP I2S LRCLK | MAX98357A I2S word select |
| P0.20 | LED STRIP PWM | WS2812B data out to desk strip (backup) |
| P0.21 | QSPI CLK | 16MB QSPI Flash |
| P0.22–P0.25 | QSPI DATA[0:3] | 16MB QSPI Flash |
| P0.26 | BUTTON_1 | Dismiss break |
| P0.27 | BUTTON_2 | Toggle sit/stand |
| P0.28 | BUTTON_3 | Mute alerts |
| P0.29 | BUTTON_4 | Pair new node |
| P0.30 | CHARGER_IRQ | Battery charger interrupt |
| P0.31 | VBUS_SENSE | USB power detection |
| P1.00 | LIPO_ADC | Battery voltage monitoring |
| P1.01 | HAPTIC_EN | Enable DRV2605L haptic driver |

**Hub firmware responsibilities:**
- BLE mesh network coordinator (manages connections to all nodes)
- mmWave radar signal processing (range-Doppler → skeleton pose estimation)
- Local ML inference (TensorFlow Lite Micro — posture classification, RSI risk scoring)
- I2C sensor polling (ambient light, temp/humidity)
- RGBW ambient lighting control (circadian schedule + manual override)
- Audio playback for break reminders, focus sounds
- Data aggregation and time-series buffering to SD card
- WiFi uplink via ESP32-C6 (MQTT QoS 1, TLS 1.3)
- BLE GATT server for mobile app real-time connection
- OLED dashboard rendering (posture score, break timer, desk height)
- OTA update management for all mesh nodes
- Ultradian rhythm tracking and break scheduling algorithm

### 2. Chair Pad Node (1 per workstation)

Sits under the user's seat cushion and behind the backrest. Maps pressure distribution to detect slouching, leaning, asymmetric sitting, and prolonged static posture.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52832 | BLE mesh node + pressure sensor scanning |
| Pressure Sensors | 16× FSR-402 (Interlink) | 8× seat + 8× backrest pressure grid |
| IMU | LSM6DSOX (I2C) | Chair tilt/movement detection |
| Multiplexer | 2× CD74HC4067 | 16-channel analog mux for 16 FSRs |
| ADC | ADS1115 (I2C, 16-bit) | Precision pressure measurement |
| Power | 1000mAh LiPo + BQ24072 | USB-C charging, ~72h battery life |
| Charging | USB-C | Trickle charge during use |
| Vibration | ERM motor + DRV2605L | Sit-to-stand nudge alerts |
| Status LED | WS2812B | Single RGB LED for connection/battery status |

**Pressure Grid Layout:**
```
BACKREST (8 sensors)          SEAT (8 sensors)
┌────────────────────┐        ┌────────────────────┐
│  B1  B2  B3  B4  │        │  S1  S2  S3  S4   │
│                   │        │                    │
│  B5  B6  B7  B8  │        │  S5  S6  S7  S8   │
└────────────────────┘        └────────────────────┘
 Upper    Lower                Left   Center  Right
 back     back                thigh   seat   thigh
```

**Pin Assignments (nRF52832):**

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | I2C SDA | ADS1115, LSM6DSOX, DRV2605L |
| P0.03 | I2C SCL | Shared I2C bus @ 100kHz (power constrained) |
| P0.04 | MUX1_EN | Enable backrest multiplexer |
| P0.05 | MUX2_EN | Enable seat multiplexer |
| P0.06–P0.09 | MUX_ADDR[0:3] | 4-bit address for both muxes |
| P0.10 | ADS1115_ALRT | ADC ready interrupt |
| P0.11 | IMU_INT1 | LSM6DSOX interrupt 1 (motion wake) |
| P0.12 | IMU_INT2 | LSM6DSOX interrupt 2 (inactivity) |
| P0.13 | HAPTIC_EN | Enable vibration motor driver |
| P0.14 | LED_DATA | WS2812B status LED |
| P0.15 | CHARGE_STAT | BQ24072 charge status |
| P0.16 | LIPO_ADC | Battery voltage via voltage divider |
| P0.17 | VBUS_SENSE | USB power present |

**Chair pad firmware responsibilities:**
- Periodic pressure grid scanning (every 200ms — 5Hz)
- Pressure map compression (16 values → posture classification feature vector)
- IMU motion/inactivity detection for sit/stand transitions
- BLE mesh message TX (pressure map + IMU data + battery level)
- Local slouch detection (threshold-based for <100ms response)
- Haptic alert for prolonged static posture (>30min continuous sitting)
- Low-power sleep between scans (target: <1mA average)
- Battery monitoring and low-power mode degradation

### 3. Desk Controller Node (1 per workstation)

Mounts to the desk frame. Controls the height-adjustable desk mechanism and monitor tilt actuator. Drives ambient lighting.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32G070CB | BLE mesh node + motor control + sensor I/O |
| BLE Radio | nRF52810 (sub-module) | BLE mesh connectivity |
| Motor Driver | 2× DRV8871 (H-bridge) | Desk lift motor control (up/down) |
| Actuator Driver | Servo PWM (PCA9685) | Monitor tilt actuator control |
| Linear Actuator | PA-14-150-12V | 150mm stroke desk height adjustment |
| Monitor Tilt | SG90 servo + 3D printed mount | ±15° monitor tilt adjustment |
| Current Sense | INA219 (I2C) | Desk motor current monitoring (stall detection) |
| Hall Effect | 2× A3144 | Desk height position feedback (magnetic strip) |
| Ambient Light Strip | WS2812B (1m, 60LEDs/m) | Circadian desk lighting |
| LED Driver | 5V 3A buck + LP5562 | LED strip power management |
| Power | 12V 5A brick + 3.3V LDO | Main power from desk power supply |
| USB | USB-C (power only) | 5V phone charging pass-through |
| Buttons | 2× tactile | Manual up/down (fallback) |
| Endstop | 2× mechanical | Top/bottom travel limits |

**Pin Assignments (STM32G070CB):**

| Pin | Function | Notes |
|-----|----------|-------|
| PA0 | ADC1_IN0 | Hall sensor 1 analog (height position) |
| PA1 | ADC1_IN1 | Hall sensor 2 analog (height position) |
| PA2 | UART_TX | → nRF52810 BLE module |
| PA3 | UART_RX | ← nRF52810 BLE module |
| PA4 | DAC1_OUT | Desk motor speed reference (0–3.3V) |
| PA5 | SPI1_SCK | nRF52810 SPI CLK (OTA) |
| PA6 | SPI1_MISO | nRF52810 SPI MISO |
| PA7 | SPI1_MOSI | nRF52810 SPI MOSI |
| PA8 | I2C3_SCL | INA219, PCA9685, LP5562 |
| PA9 | I2C3_SDA | Shared I2C bus @ 400kHz |
| PA10 | TIM1_CH1 | Motor 1 PWM (desk up) — DRV8871 IN1 |
| PA11 | TIM1_CH2 | Motor 1 PWM (desk down) — DRV8871 IN2 |
| PA12 | TIM1_CH3 | Motor 2 PWM (monitor tilt) — DRV8871 IN1 |
| PA13 | TIM1_CH4 | Motor 2 PWM (monitor tilt) — DRV8871 IN2 |
| PA14 | LED_STRIP_DATA | WS2812B data line |
| PA15 | SERVO_PWM | Monitor tilt servo (via PCA9685 backup) |
| PB0 | BUTTON_UP | Manual desk up |
| PB1 | BUTTON_DOWN | Manual desk down |
| PB2 | ENDSTOP_TOP | Top travel limit |
| PB3 | ENDSTOP_BOTTOM | Bottom travel limit |
| PB4 | MOTOR_EN | Motor driver enable |
| PB5 | CURRENT_ALERT | INA219 alert interrupt |
| PB6 | NRF_RESET | BLE module reset |
| PB7 | NRF_BOOT | BLE module boot mode |
| PB8 | VBUS_5V | 5V USB charging detect |
| PB9 | LED_STATUS | Onboard status LED |
| PC6 | TIM3_CH1 | Servo PWM output (direct) |
| PC7 | TIM3_CH2 | Servo PWM output (direct) |

**Desk controller firmware responsibilities:**
- BLE mesh message RX: desk height commands, monitor tilt commands, lighting commands
- Closed-loop PID desk height control using Hall sensor feedback
- Motor current monitoring for stall/obstruction detection (safety)
- Endstop detection for travel limits (hardware interrupt → immediate motor stop)
- Monitor tilt servo positioning (±15° range)
- WS2812B ambient lighting control (circadian schedule from hub)
- Local manual control (up/down buttons — hardware override)
- Smooth motion profiles (acceleration/deceleration ramp — no jerky desk movements)
- Position memory (3 preset heights: sit, stand, custom)
- OTA update via BLE mesh from hub

### 4. Wearable Tag Node (1-2 per workstation)

Wrist-worn or lanyard-worn IMU tag. Tracks micro-movements, typing patterns, and gesture recognition for RSI risk detection. Optional pulse oximetry for heart rate monitoring.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52833 | BLE mesh node + IMU processing |
| IMU | ICM-42688-P (SPI) | 6-axis accel/gyro, high-precision motion tracking |
| Magnetometer | MMC5603 (I2C) | 3-axis heading for 9-DOF fusion |
| Pulse Ox | MAX30101 (I2C) | Heart rate + SpO2 (optional health monitoring) |
| Vibration | ERM motor + transistor driver | Wrist haptic alerts for breaks |
| LED | Single WS2812B | Status: connection, battery, posture alert |
| Battery | 150mAh LiPo | 48h typical battery life |
| Charger | MCP73831 | USB-C charging |
| Power Mgmt | nRF PMU + DCDC | Ultra-low power sleep modes |
| Antenna | Custom 2.4GHz PCB antenna | Optimized for wrist form factor |

**Pin Assignments (nRF52833):**

| Pin | Function | Notes |
|-----|----------|-------|
| P0.02 | I2C SDA | MMC5603, MAX30101 |
| P0.03 | I2C SCL | Shared I2C @ 100kHz |
| P0.04 | SPI CLK | ICM-42688-P SPI bus |
| P0.05 | SPI MOSI | ICM-42688-P SPI |
| P0.06 | SPI MISO | ICM-42688-P SPI |
| P0.07 | SPI CS (IMU) | ICM-42688-P chip select |
| P0.08 | IMU_INT1 | ICM-42688-P data-ready interrupt |
| P0.09 | IMU_INT2 | ICM-42688-P tap/activity interrupt |
| P0.10 | PULSE_INT | MAX30101 interrupt |
| P0.11 | HAPTIC_PWM | ERM motor drive (PWM) |
| P0.12 | LED_DATA | WS2812B status LED |
| P0.13 | CHARGE_STAT | MCP73831 charge status |
| P0.14 | LIPO_ADC | Battery voltage monitor |
| P0.15 | PULSE_EN | Enable MAX30101 (power gating) |

**Wearable tag firmware responsibilities:**
- IMU data acquisition at 100Hz (6-axis: 3 accel + 3 gyro)
- Magnetometer acquisition at 25Hz
- 9-DOF sensor fusion (Madgwick filter) for orientation tracking
- Gesture recognition (typing detection, mouse use, reaching, stretching)
- Activity classification (typing, mousing, phone use, idle, stretching)
- Heart rate sampling (every 60s, on-demand via MAX30101)
- BLE mesh message TX (orientation + activity + HR)
- Haptic alert patterns (3 vibration patterns: gentle break, urgent break, posture warning)
- Ultra-low-power scheduling (IMU at 100Hz, BLE TX every 500ms, HR every 60s)
- Tap detection for UI: double-tap to dismiss alert, triple-tap for status

### 5. Cloud / Edge (Software)

See `software/` directory for full implementation details.

- **FastAPI backend** — REST API + MQTT bridge + WebSocket for real-time dashboard
- **ML pipeline** — PyTorch models: posture CNN, RSI risk LSTM, focus detector, circadian optimizer
- **Mobile app** — React Native: live posture score, break reminders with AR stretch guides, weekly health reports

---

## Communication Protocol

### BLE Mesh Network

All nodes communicate over BLE mesh (2.4GHz). The Hub acts as the Provisioner.

| Parameter | Value |
|-----------|-------|
| Protocol | Bluetooth Mesh 1.0.1 |
| Bearer | ADV bearer (advertising channels 37, 38, 39) |
| Security | AES-CCM 128-bit, provisioning via OOB numeric |
| Network | 1 subnets, 2 application keys (control, telemetry) |
| Addressing | Hub: 0x0001, Chair: 0x0002, Desk: 0x0003, Tag₁: 0x0004, Tag₂: 0x0005 |
| TTL | 7 hops (more than enough for single-room) |
| Retransmit | 3 retries, 100ms interval |
| Relay | Hub only (star topology, hub relays between nodes) |

### Message Types

| Opcode | Name | Direction | Payload | Description |
|--------|------|-----------|---------|-------------|
| 0xC001 | PRESSURE_MAP | Chair → Hub | 16× uint8 pressure values (0–255), 1 IMU flag byte | Full pressure grid + sit/stand state |
| 0xC002 | IMU_ORIENTATION | Tag → Hub | 4× float (quaternion), 1 byte activity class, 1 byte confidence | Wearable orientation + activity |
| 0xC003 | HEART_RATE | Tag → Hub | 1 byte HR (bpm), 1 byte SpO2 (%) | Heart rate + oxygen saturation |
| 0xC004 | DESK_COMMAND | Hub → Desk | 1 byte cmd, 2 bytes target_height_mm, 1 byte speed_pct | Height/tilt/light command |
| 0xC005 | DESK_STATUS | Desk → Hub | 2 bytes height_mm, 1 byte motor_state, 2 bytes current_mA | Desk position and motor status |
| 0xC006 | AMBIENT_READING | Hub → Cloud | 4 bytes lux, 2 bytes temp_c, 2 bytes humidity_pct | Environmental sensor data |
| 0xC007 | POSTURE_SCORE | Hub → All | 1 byte score (0–100), 1 byte risk_level, 2 bytes duration_s | Current posture assessment |
| 0xC008 | BREAK_REMINDER | Hub → All | 1 byte type (stretch/walk/look_away), 2 bytes duration_s | Break nudge command |
| 0xC009 | LIGHTING_CMD | Hub → Desk | 4 bytes (R,G,B,W), 1 byte brightness_pct, 1 byte mode | Ambient lighting set command |
| 0xC00A | MONITOR_TILT | Hub → Desk | 1 byte tilt_degrees (-15 to +15), 1 byte speed_pct | Monitor angle adjustment |
| 0xC00B | OTA_AVAILABLE | Hub → All | 4 bytes firmware_size, 16 bytes sha256_hash | New firmware announcement |
| 0xC00C | OTA_DATA | Hub → Node | 2 bytes seq_num, 16 bytes chunk | Firmware data chunk |
| 0xC00D | NODE_HEARTBEAT | All → Hub | 1 byte batt_pct, 1 byte state, 2 bytes uptime_min | Health check |
| 0xC00E | CALIBRATION | Hub → Any | 1 byte target, 4 bytes param1, 4 bytes param2 | Calibration command |
| 0xC00F | FACTORY_RESET | Hub → Any | (empty) | Reset node to defaults |

### Hub ↔ ESP32-C6 UART Protocol

| Field | Size | Description |
|-------|------|-------------|
| Sync | 2 bytes | 0xAA55 |
| Length | 2 bytes | Payload length (little-endian) |
| Opcode | 1 byte | Command/response type |
| Payload | N bytes | Variable |
| CRC16 | 2 bytes | CCITT CRC over all preceding bytes |

Opcodes: 0x01=MQTT_PUBLISH, 0x02=MQTT_SUBSCRIBE, 0x03=WIFI_SCAN, 0x04=WIFI_CONNECT, 0x05=OTA_DATA, 0x06=HTTP_GET, 0x81=MQTT_MESSAGE, 0x82=WIFI_STATUS, 0x83=OTA_PROGRESS, 0x84=HTTP_RESPONSE

---

## Power Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                        POWER ARCHITECTURE                         │
│                                                                   │
│  WALL OUTLET (12V 5A)                                            │
│       │                                                          │
│       ├──► DESK CONTROLLER (12V direct)                           │
│       │    ├── 5V 3A buck → LED strip, USB charging              │
│       │    └── 3.3V LDO → MCU, logic, sensors                    │
│       │                                                          │
│       └──► HUB NODE (5V USB-C)                                    │
│            ├── 3.3V LDO → nRF5340, ESP32-C6, sensors            │
│            ├── LiPo (2000mAh) → backup power                     │
│            └── BQ24072 → charge/discharge management             │
│                                                                   │
│  CHAIR PAD (self-powered)                                         │
│       ├── LiPo 1000mAh → 3.3V buck-boost                        │
│       ├── USB-C → BQ24072 → LiPo charging                       │
│       └── Average: ~1.5mA (BLE mesh + periodic scan)             │
│           Battery life: ~72 hours continuous                     │
│                                                                   │
│  WEARABLE TAG (self-powered)                                      │
│       ├── LiPo 150mAh → 3.3V DCDC                                │
│       ├── USB-C → MCP73831 → LiPo charging                      │
│       └── Average: ~3mA (IMU 100Hz + BLE TX 2Hz)               │
│           Battery life: ~48 hours continuous                     │
│                                                                   │
│  POWER BUDGET (worst case, all nodes active):                     │
│       Hub:          ~180mA @ 3.3V = 594mW                       │
│       Chair pad:    ~1.5mA @ 3.3V = 5mW                         │
│       Desk ctrl:   ~2A @ 12V (motors) = 24W peak               │
│       Wearable tag: ~3mA @ 3.3V = 10mW                         │
│       Total (typical): ~26W peak (desk moving), ~3W idle        │
└──────────────────────────────────────────────────────────────────┘
```

---

## Machine Learning Pipeline

### Models

| Model | Architecture | Input | Output | Purpose |
|-------|-------------|-------|--------|---------|
| PostureNet | 1D-CNN (3 conv + 2 FC) | 16× pressure map + 6× IMU (22 features) | 5 classes (good, slouch, lean-L, lean-R, hunch) | Real-time posture classification |
| RSI-Risk | LSTM (2-layer, 128-hidden) | 180-step window of PostureNet outputs + activity vector | Risk score 0–100 | Cumulative RSI risk prediction |
| FocusNet | Temporal CNN | mmWave range-Doppler + IMU motion variance | Focus level (low/medium/high) | Ultradian rhythm tracking |
| CircadianLight | Gradient-boosted trees | Time-of-day, lux history, user preference | RGBW values | Optimal circadian lighting |
| ActivityNet | 1D-CNN + attention | 9-DOF IMU @ 100Hz, 2s windows | 6 classes (typing, mouse, phone, idle, stretch, walk) | Micro-activity recognition |

### Training Pipeline

1. **Data collection**: Raw sensor data streamed to cloud via MQTT
2. **Labeling**: Semi-automated labeling using video ground truth + active learning
3. **PostureNet**: Train on pressure map + IMU data, 5-class classification, ~50K samples
4. **RSI-Risk**: Train on 2-week activity sequences, predict next-week risk score, ~10K sequences
5. **FocusNet**: Train on mmWave + IMU data, 3-class focus level, ~20K labeled windows
6. **CircadianLight**: Train on user preference + lux data, ~5K user-sessions
7. **ActivityNet**: Train on 9-DOF IMU data, 6-class activity, ~100K labeled windows
8. **Edge deployment**: Convert to TFLite Micro, quantize to int8, deploy to hub via OTA

### Inference Schedule

| Model | Platform | Frequency | Latency |
|-------|----------|-----------|---------|
| PostureNet | Hub (nRF5340) | Every 500ms | <15ms |
| ActivityNet | Wearable tag (nRF52833) | Every 1s | <10ms |
| RSI-Risk | Hub | Every 60s | <50ms |
| FocusNet | Hub | Every 10s | <30ms |
| CircadianLight | Hub | Every 5min | <5ms |

---

## API Endpoints (FastAPI Backend)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/status` | System status (all nodes online, battery levels) |
| GET | `/api/v1/posture/current` | Current posture score and class |
| GET | `/api/v1/posture/history` | Posture score time series |
| GET | `/api/v1/rsi-risk` | Current RSI risk assessment |
| GET | `/api/v1/rsi-risk/trends` | RSI risk trends over time |
| GET | `/api/v1/activity/current` | Current detected activity |
| GET | `/api/v1/activity/stats` | Activity breakdown (today, week, month) |
| GET | `/api/v1/focus/current` | Current focus level |
| GET | `/api/v1/focus/sessions` | Focus session history |
| GET | `/api/v1/breaks` | Break history and compliance |
| POST | `/api/v1/breaks/dismiss` | Dismiss current break reminder |
| POST | `/api/v1/desk/height` | Set desk height (mm) |
| POST | `/api/v1/desk/preset` | Set desk to preset (sit/stand/custom) |
| POST | `/api/v1/lighting` | Set ambient lighting (RGBW) |
| POST | `/api/v1/monitor/tilt` | Set monitor tilt angle |
| GET | `/api/v1/environment` | Current lux, temp, humidity |
| GET | `/api/v1/analytics/weekly` | Weekly health report |
| POST | `/api/v1/calibrate` | Start calibration sequence |
| GET | `/api/v1/nodes` | List all mesh nodes and status |
| POST | `/api/v1/ota/start` | Initiate OTA update |
| WS | `/ws/v1/realtime` | WebSocket for real-time posture/activity stream |

---

## Mobile App Screens

1. **Dashboard** — Live posture score (large circular gauge), current activity icon, break timer, desk height indicator
2. **Posture History** — Daily/weekly posture score charts, slouch event timeline, improvement trends
3. **Break Reminders** — Stretch guide with AR overlay (phone camera tracks your stretch form), hydration log
4. **Desk Control** — Sit/stand toggle, height slider, monitor tilt slider, preset buttons, ambient lighting wheel
5. **Focus Sessions** — Focus score over time, Pomodoro-style timer synced with focus detection, distraction alerts
6. **Weekly Report** — Posture grade (A–F), break compliance %, focus hours, RSI risk trend, personalized recommendations
7. **Settings** — Node pairing, calibration, break preferences, lighting schedule, notification settings
8. **Node Management** — Battery levels, connection status, firmware versions, OTA update trigger

---

## BOM Summary

### Hub Node

| # | Component | Part | Qty | Unit Price | Total |
|---|-----------|------|-----|-----------|-------|
| 1 | MCU | nRF5340 | 1 | $5.20 | $5.20 |
| 2 | WiFi Co-MCU | ESP32-C6-MINI-1 | 1 | $2.50 | $2.50 |
| 3 | mmWave Radar | BGT60TR13C | 1 | $8.90 | $8.90 |
| 4 | Ambient Light | TSL2591 | 1 | $2.80 | $2.80 |
| 5 | Temp/Humidity | SHT40-AD1B | 1 | $1.90 | $1.90 |
| 6 | LED Driver | LP5562 | 1 | $0.95 | $0.95 |
| 7 | MEMS Mic | SPH0645LM4H | 1 | $2.40 | $2.40 |
| 8 | Audio Amp | MAX98357A | 1 | $1.20 | $1.20 |
| 9 | OLED | SH1106 1.3" | 1 | $3.50 | $3.50 |
| 10 | Haptic Driver | DRV2605L | 1 | $1.10 | $1.10 |
| 11 | Flash | W25Q128 (16MB) | 1 | $0.80 | $0.80 |
| 12 | SD Slot | MicroSD holder | 1 | $0.30 | $0.30 |
| 13 | LiPo | 2000mAh 3.7V | 1 | $4.50 | $4.50 |
| 14 | Charger IC | BQ24072 | 1 | $1.10 | $1.10 |
| 15 | LDO | TLV1117-3.3 | 2 | $0.30 | $0.60 |
| 16 | USB-C | USB-C receptacle | 1 | $0.40 | $0.40 |
| 17 | Buttons | Tactile 6x6mm | 4 | $0.05 | $0.20 |
| 18 | Connectors | StemmaQT 4-pin | 2 | $0.50 | $1.00 |
| 19 | PCB | 4-layer 80x50mm | 1 | $2.50 | $2.50 |
| 20 | Antenna | 2.4GHz chip antenna | 1 | $0.40 | $0.40 |
| 21 | Passives | R/C/L assort | 1 | $1.50 | $1.50 |
| 22 | Enclosure | 3D printed PLA | 1 | $3.00 | $3.00 |
| | | | | **Total** | **$46.65** |

### Chair Pad Node

| # | Component | Part | Qty | Unit Price | Total |
|---|-----------|------|-----|-----------|-------|
| 1 | MCU | nRF52832 | 1 | $3.60 | $3.60 |
| 2 | Pressure Sensors | FSR-402 | 16 | $1.20 | $19.20 |
| 3 | Multiplexer | CD74HC4067 | 2 | $0.70 | $1.40 |
| 4 | ADC | ADS1115 | 1 | $2.10 | $2.10 |
| 5 | IMU | LSM6DSOX | 1 | $2.80 | $2.80 |
| 6 | Haptic | ERM motor + DRV2605L | 1 | $1.60 | $1.60 |
| 7 | LED | WS2812B | 1 | $0.15 | $0.15 |
| 8 | LiPo | 1000mAh 3.7V | 1 | $3.50 | $3.50 |
| 9 | Charger IC | BQ24072 | 1 | $1.10 | $1.10 |
| 10 | USB-C | USB-C receptacle | 1 | $0.40 | $0.40 |
| 11 | Voltage Reg | TLV62569 (buck-boost) | 1 | $0.80 | $0.80 |
| 12 | PCB | 4-layer 200x150mm | 1 | $5.00 | $5.00 |
| 13 | Antenna | PCB trace antenna | 1 | $0.00 | $0.00 |
| 14 | Passives | R/C/L assort | 1 | $1.50 | $1.50 |
| 15 | Enclosure | Flexible TPU pad | 1 | $4.00 | $4.00 |
| | | | | **Total** | **$50.55** |

### Desk Controller Node

| # | Component | Part | Qty | Unit Price | Total |
|---|-----------|------|-----|-----------|-------|
| 1 | MCU | STM32G070CB | 1 | $2.30 | $2.30 |
| 2 | BLE Module | nRF52810 sub-module | 1 | $2.80 | $2.80 |
| 3 | Motor Driver | DRV8871 | 2 | $1.90 | $3.80 |
| 4 | LED Strip | WS2812B (1m, 60LED) | 1 | $4.50 | $4.50 |
| 5 | LED Driver | LP5562 | 1 | $0.95 | $0.95 |
| 6 | Servo Driver | PCA9685 | 1 | $1.20 | $1.20 |
| 7 | Servo | SG90 + 3D mount | 1 | $2.50 | $2.50 |
| 8 | Current Sense | INA219 | 1 | $1.50 | $1.50 |
| 9 | Hall Sensors | A3144 | 2 | $0.30 | $0.60 |
| 10 | Buck Conv | LM2596 (5V 3A) | 1 | $1.20 | $1.20 |
| 11 | LDO | TLV1117-3.3 | 1 | $0.30 | $0.30 |
| 12 | Endstops | Mechanical switch | 2 | $0.20 | $0.40 |
| 13 | Buttons | Tactile 6x6mm | 2 | $0.05 | $0.10 |
| 14 | Linear Actuator | PA-14-150-12V | 1 | $18.00 | $18.00 |
| 15 | Power Supply | 12V 5A brick | 1 | $6.00 | $6.00 |
| 16 | PCB | 4-layer 80x60mm | 1 | $3.00 | $3.00 |
| 17 | Antenna | 2.4GHz chip antenna | 1 | $0.40 | $0.40 |
| 18 | Passives | R/C/L assort | 1 | $2.00 | $2.00 |
| 19 | Enclosure | 3D printed PLA | 1 | $3.00 | $3.00 |
| | | | | **Total** | **$54.55** |

### Wearable Tag Node

| # | Component | Part | Qty | Unit Price | Total |
|---|-----------|------|-----|-----------|-------|
| 1 | MCU | nRF52833 | 1 | $3.80 | $3.80 |
| 2 | IMU | ICM-42688-P | 1 | $3.20 | $3.20 |
| 3 | Magnetometer | MMC5603 | 1 | $1.40 | $1.40 |
| 4 | Pulse Ox | MAX30101 | 1 | $4.50 | $4.50 |
| 5 | ERM Motor | 4mm coin vibe | 1 | $0.60 | $0.60 |
| 6 | LED | WS2812B | 1 | $0.15 | $0.15 |
| 7 | LiPo | 150mAh 3.7V | 1 | $2.80 | $2.80 |
| 8 | Charger IC | MCP73831 | 1 | $0.60 | $0.60 |
| 9 | DC-DC | TPS62740 (buck) | 1 | $0.90 | $0.90 |
| 10 | USB-C | USB-C receptacle | 1 | $0.40 | $0.40 |
| 11 | PCB | 4-layer 35x25mm | 1 | $2.00 | $2.00 |
| 12 | Antenna | 2.4GHz chip antenna | 1 | $0.40 | $0.40 |
| 13 | Passives | R/C/L assort | 1 | $1.00 | $1.00 |
| 14 | Enclosure | 3D printed resin | 1 | $2.50 | $2.50 |
| 15 | Strap | Silicone band | 1 | $1.50 | $1.50 |
| | | | | **Total** | **$23.75** |

### System Total (4 nodes)

| Node | Qty | Unit Cost | Total |
|------|-----|-----------|-------|
| Hub | 1 | $46.65 | $46.65 |
| Chair Pad | 1 | $50.55 | $50.55 |
| Desk Controller | 1 | $54.55 | $54.55 |
| Wearable Tag | 1 | $23.75 | $23.75 |
| | | **System Total** | **$175.50** |

Retail price target: $299 (complete system)

---

## Firmware Architecture

### Common Code (shared across all nodes)

```
firmware/common/
├── ble_mesh/
│   ├── mesh_config.h          — Mesh network configuration (netkeys, appkeys, addresses)
│   ├── mesh_handler.c         — BLE mesh message dispatch
│   ├── mesh_handler.h
│   ├── protocol.h             — Message opcodes, structures, serialization
│   └── protocol.c             — Pack/unpack message payloads
├── sensors/
│   ├── i2c_bus.c              — Shared I2C bus initialization and scan
│   ├── i2c_bus.h
│   ├── spi_bus.c              — Shared SPI bus initialization
│   ├── spi_bus.h
│   ├── ads1115.c              — ADC driver (chair pad)
│   ├── ads1115.h
│   ├── lsm6dsox.c             — IMU driver (chair pad)
│   ├── lsm6dsox.h
│   ├── icm42688.c             — IMU driver (wearable tag)
│   ├── icm42688.h
│   ├── mmc5603.c              — Magnetometer driver (wearable tag)
│   ├── mmc5603.h
│   ├── max30101.c             — Pulse ox driver (wearable tag)
│   ├── max30101.h
│   ├── tsl2591.c              — Ambient light driver (hub)
│   ├── tsl2591.h
│   ├── sht40.c                — Temp/humidity driver (hub)
│   ├── sht40.h
│   └── bgt60tr13c.c           — mmWave radar driver (hub)
│   └── bgt60tr13c.h
├── power/
│   ├── battery.c              — Battery monitoring (ADC + voltage divider)
│   ├── battery.h
│   ├── charger.c              — Charge IC interface
│   └── charger.h
├── util/
│   ├── crc16.c                — CCITT CRC16
│   ├── crc16.h
│   ├── ringbuf.c              — Lock-free ring buffer
│   └── ringbuf.h
└── board/
    ├── nrf5340_common.c        — nRF5340 board init
    ├── nrf52832_common.c       — nRF52832 board init
    ├── nrf52833_common.c       — nRF52833 board init
    └── stm32g070_common.c      — STM32G070 board init
```

### Node-Specific Code

```
firmware/
├── hub-node/
│   ├── main.c                  — Entry point, task scheduler
│   ├── mesh_coordinator.c     — BLE mesh provisioner logic
│   ├── radar_processor.c      — mmWave signal processing
│   ├── posture_engine.c       — Local posture classification (TFLite Micro)
│   ├── focus_detector.c       — Focus level detection
│   ├── break_scheduler.c      — Ultradian rhythm break scheduler
│   ├── lighting_ctrl.c        — Circadian lighting algorithm
│   ├── audio_player.c         — Break reminder audio playback
│   ├── oled_ui.c              — OLED dashboard rendering
│   ├── esp32_bridge.c          — UART protocol to ESP32-C6
│   └── CMakeLists.txt
├── chair-pad/
│   ├── main.c                  — Entry point, power management
│   ├── pressure_scanner.c      — FSR grid scanning + mux control
│   ├── imu_handler.c           — IMU data acquisition
│   ├── posture_local.c         — Simple threshold posture detection
│   ├── haptic_alert.c          — Vibration alert patterns
│   └── CMakeLists.txt
├── desk-controller/
│   ├── main.c                  — Entry point, motor control loop
│   ├── motor_pid.c             — PID closed-loop height control
│   ├── hall_position.c         — Hall sensor position decoding
│   ├── current_monitor.c       — INA219 current monitoring
│   ├── servo_ctrl.c            — Monitor tilt servo control
│   ├── led_strip.c             — WS2812B ambient lighting
│   ├── endstop.c               — Endstop interrupt handlers
│   └── CMakeLists.txt
├── wearable-tag/
│   ├── main.c                  — Entry point, ultra-low-power scheduler
│   ├── imu_acquisition.c       — 100Hz IMU data acquisition
│   ├── sensor_fusion.c         — 9-DOF Madgwick filter
│   ├── activity_classifier.c   — Activity detection (simple model)
│   ├── heart_rate.c            — MAX30101 HR sampling
│   ├── haptic_patterns.c       — Vibration pattern definitions
│   └── CMakeLists.txt
└── common/                      — (shared code, see above)
```

---

## Calibration & Setup

### First-Time Setup

1. **Hub pairing**: Power on hub → mobile app discovers via BLE → WiFi credentials → MQTT broker config
2. **Chair pad pairing**: Place on chair → press pair button → hub auto-discovers via BLE mesh
3. **Desk controller pairing**: Connect to desk → press pair button → hub discovers
4. **Wearable tag pairing**: Power on → double-tap to enter pairing → hub discovers
5. **Calibration sequence**:
   - Sit naturally for 10 seconds (pressure baseline)
   - Stand up for 5 seconds (zero reference)
   - Slouch deliberately for 5 seconds (slouch baseline)
   - Lean left for 5 seconds
   - Lean right for 5 seconds
   - Desk height calibration: sit height → stand height → save presets

### Ongoing Calibration

- Pressure sensors auto-zero every power cycle
- IMU drift correction via Madgwick filter with periodic magnetometer correction
- Desk position recalibration via endstops every 50 cycles
- ML model weights updated weekly via OTA from cloud

---

## Safety Features

1. **Desk collision detection**: Motor current monitoring (INA219) detects stall/obstruction → immediate motor stop
2. **Endstop hard limits**: Hardware interrupts → immediate motor cutoff regardless of firmware state
3. **Manual override**: Physical up/down buttons always functional, bypasses BLE commands
4. **Watchdog timer**: All nodes have 30-second watchdog → auto-reboot on hang
5. **BLE mesh timeout**: If hub goes offline, nodes continue local operation (chair pad still vibrates on slouch, desk responds to buttons)
6. **Heart rate alerts**: Wearable tag sends urgent BLE message if HR exceeds configurable threshold
7. **Battery protection**: All LiPo cells have BMS with over-charge, over-discharge, over-current protection
8. **Thermal shutdown**: MCU temperature sensors trigger safe shutdown at 85°C

---

## Future Extensions

- **Monitor arm actuator**: Motorized monitor height adjustment (vertical)
- **Keyboard tilt**: Motorized keyboard tray angle adjustment
- **Footrest**: Heated/cooled footrest with pressure sensing
- **Voice control**: Integration with Alexa/Google Home for "stand up" / "sit down" commands
- **Health API**: Integration with Apple Health / Google Fit for posture data
- **Multi-user profiles**: Automatic user detection via wearable tag, personalized settings
- **Therapist portal**: Remote access for physical therapists to review patient data
- **Standing mat**: Pressure-sensing floor mat for standing posture analysis

---

## License

MIT — build it, sell it, improve it.

---

*Invented by [jayis1](https://github.com/jayis1). Part of the [Devices](https://github.com/jayis1/Devices) project.*