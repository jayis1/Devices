# MobilitySync

**AI-powered home mobility assistance, transfer safety, and fatigue-aware accessibility system** — a multi-node platform for people who use walkers, wheelchairs, canes, or caregiver support at home. MobilitySync combines a room-aware hub, a sensor-rich smart walker, a pressure-sensing transfer mat, an automatic doorway controller, and a wearable safety band to reduce falls, make transfers safer, and remove friction from moving through the home.

## What It Solves

Millions of people lose independence at home because everyday motion is full of small hazards:

- standing up from a bed or chair without enough forward weight shift;
- starting to walk before balance is recovered;
- forgetting to lock a walker or engage a brake on a sloped threshold;
- struggling with heavy interior doors while using a mobility aid;
- overexerting during repeated room-to-room trips, causing fatigue and fall risk later in the day;
- caregivers lacking objective data about near-falls, safe transfers, and mobility decline.

MobilitySync closes the loop from **sensing -> prediction -> intervention -> caregiver visibility**.

---

## System Architecture

```text
┌────────────────────────────────────────────────────────────────────────────────────────────┐
│                        MobilitySync Edge + Cloud Intelligence Stack                         │
│ FastAPI + MQTT + PostgreSQL + object storage + clinician reports + OTA                    │
│                                                                                            │
│ Models                                                                                     │
│ • TransferNet TCN         - safe / unsafe sit-to-stand and bed-transfer detection         │
│ • WalkerBrakeNet 1D-CNN   - imminent loss-of-balance and runaway walker prediction        │
│ • FatigueForecaster TFT   - 2 h mobility fatigue and assistance need forecast             │
│ • RouteClear XGBoost      - doorway congestion / best path / automatic opener timing      │
│ • PressureRelief LSTM     - offloading and seated pressure injury risk                    │
│ • RecoveryTrend Bayesian  - week-over-week mobility decline / improvement estimation      │
└────────────────────────────────────────────┬───────────────────────────────────────────────┘
                                             │ MQTT over TLS / HTTPS
                                   ┌─────────┴──────────┐
                                   │ Mobility Hub       │
                                   │ CM4 + RP2040 +     │
                                   │ SX1262 + DW3110    │
                                   └──────┬──────┬──────┘
                                          │      │
                       Sub-GHz 868 TDMA   │      │ BLE 5.3 + Wi-Fi commissioning
                                          │      │
             ┌────────────────────────────┼──────┼──────────────────────────────┐
             │                            │      │                              │
     ┌───────┴────────┐         ┌─────────┴──────┐                  ┌──────────┴──────────┐
     │ Smart Walker   │         │ Transfer Mat   │                  │ Doorway Controller   │
     │ forces + IMU + │         │ pressure map + │                  │ door opener + UWB    │
     │ brakes + UWB   │         │ mmWave + load  │                  │ anchor + safety I/O  │
     └───────┬────────┘         └─────────┬──────┘                  └──────────┬──────────┘
             │                              │                                    │
             └────────────── BLE 5.3 ──────┴───────────────┬────────────────────┘
                                                            │
                                                  ┌─────────┴─────────┐
                                                  │ Wearable Band     │
                                                  │ PPG + IMU + temp  │
                                                  │ + haptics + SOS   │
                                                  └───────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **Mobility Hub** | Raspberry Pi CM4 + RP2040 + SX1262 + DW3110 | local policy engine, model inference, MQTT bridge, reports, OTA, fall escalation | 12 V / 3 A wall input + 2-cell UPS | Ethernet, Wi-Fi, BLE 5.3, Sub-GHz 868 MHz, UWB |
| **Smart Walker** | nRF5340 + SX1262 + DW3110 | handle force sensing, gait intent, anti-runaway braking, obstacle awareness, indoor navigation | 24 V Li-ion mobility pack + isolated 5 V / 3.3 V | BLE 5.3, Sub-GHz 868 MHz, UWB |
| **Transfer Mat** | ESP32-S3-WROOM-1 + IWR6843AOP + SX1262 | bed/chair transfer quality, pressure relief tracking, occupancy and stand-assist coaching | 12 VDC adapter or 4-cell LiFePO4 | Wi-Fi, BLE, Sub-GHz 868 MHz |
| **Doorway Controller** | STM32G474 + SX1262 + DW3110 | automatic door actuation, threshold illumination, latch control, arrival prediction | 24 VDC with supercap ride-through | Sub-GHz 868 MHz, UWB |
| **Wearable Band** | nRF52840 + MAX86176 | HR/HRV, SpO2 spot checks, fall confirmation, panic button, haptic cues | 250 mAh LiPo | BLE 5.3 |

---

## Daily User Experience

1. A user wakes up and shifts to the bed edge.
2. The **Transfer Mat** identifies an incomplete forward lean and asymmetric foot loading.
3. The **Wearable Band** reports elevated heart rate and low readiness after poor sleep.
4. The hub classifies the transfer as amber risk and sends a haptic cue to pause and plant both feet.
5. As the user reaches for the **Smart Walker**, handle force sensors confirm grip, the parking brakes stay engaged until weight is centered, and then release smoothly.
6. The **Doorway Controller** ahead of the walker opens the bathroom door automatically based on UWB approach direction and hallway clearance.
7. Later in the day, cumulative pushing force, transfer count, HRV suppression, and slowed cadence trigger a fatigue warning; the app suggests a rest break before another kitchen trip.
8. Caregivers receive a daily summary of safe transfers, near-falls prevented, door assist events, and mobility trend changes.

---

## Node 1 - Mobility Hub

### Core hardware

- **Compute module:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, radio scheduling, local audible alarms, and battery-safe shutdown
- **Sub-GHz radio:** Semtech SX1262
- **UWB anchor:** Qorvo DW3110 for room-level positioning and approach timing
- **Local storage:** eMMC + mirrored industrial microSD
- **User I/O:** 5 inch touchscreen, RGB status tower, mute switch, front USB-C service port
- **Backup:** 2-cell 18650 UPS for >3 h outage operation

### Responsibilities

- Maintains room graph, doorway policies, and user mobility profiles
- Fuses telemetry from walker, mat, doorway, and wearable nodes
- Executes local alerting even without internet
- Stores transfer videos? **No raw video is used**; MobilitySync is privacy-first and intentionally camera-free
- Calculates **Transfer Safety Score**, **Fatigue Score**, **Door Assist Need**, **Near-Fall Count**, and **Mobility Trend**

### Key interfaces

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 SPI1 | DW3110 UWB transceiver |
| RP2040 UART0 | CM4 heartbeat and commands |
| RP2040 I2C0 | INA219, RTC, buzzer expander |
| CM4 Ethernet | primary uplink |
| CM4 USB2 | service / export |
| CM4 DSI | touchscreen |

### Power architecture

- 12 V input -> TVS diode + resettable fuse
- MP1584EN buck -> 5 V rail for CM4 and touchscreen
- TPS62172 -> 3.3 V logic rail
- UPS charger -> protected 18650 pair
- RP2040 monitors current draw and initiates orderly shutdown below low-battery threshold

---

## Node 2 - Smart Walker

### Core function

A retrofittable or OEM walker electronics kit that senses user intent, resists runaway motion, and provides guided navigation and balance-aware braking.

### Hardware architecture

- **MCU:** Nordic nRF5340 (application + network cores)
- **Sub-GHz radio:** SX1262 for robust backbone messaging
- **UWB tag:** DW3110 for centimeter-class indoor ranging
- **IMU:** Bosch BMI270
- **Handle force sensing:** 4× half-bridge load cells into 2× HX711 ADCs
- **Wheel sensing:** dual hall quadrature encoders
- **Obstacle detection:** VL53L5CX 8x8 ToF array forward facing
- **Brake actuation:** dual 24 V fail-safe electromagnetic parking brakes driven by BTS7960 half-bridges
- **User feedback:** DRV2605L haptic puck in handles + piezo beeper + RGB strip
- **Power:** 24 V Li-ion mobility battery, 5 V buck, 3.3 V LDO, isolated brake rail monitor

### Pin assignment

| Signal | nRF5340 Pin | Peripheral |
|--------|-------------|------------|
| I2C SDA/SCL | P0.26 / P0.27 | BMI270, VL53L5CX, DRV2605L |
| SPI MOSI/MISO/SCLK | P0.20 / P0.22 / P0.21 | SX1262 |
| SX1262 CS/BUSY/DIO1/RST | P0.23 / P0.24 / P0.25 / P0.28 | radio |
| SPI2 MOSI/MISO/SCLK | P1.10 / P1.11 / P1.12 | DW3110 |
| HX711 A DOUT/SCK | P1.01 / P1.02 | left handle force |
| HX711 B DOUT/SCK | P1.03 / P1.04 | right handle force |
| Encoder L A/B | P0.10 / P0.11 | wheel encoder |
| Encoder R A/B | P0.12 / P0.13 | wheel encoder |
| Brake L/R PWM | P1.05 / P1.06 | brake bridges |
| Piezo / LED | P0.07 / P0.08 | feedback |

### Safety logic

- Brakes remain engaged until handle grip + stable COM shift + low wheel slip are confirmed
- If forward wheel acceleration exceeds user push intent, brakes pulse to prevent runaway
- Door approach and threshold crossing reduce brake aggressiveness to avoid stalling in a doorway
- Radio loss >5 s enters local-safe mode with audible prompt and conservative braking thresholds

---

## Node 3 - Transfer Mat

### Role

A thin pressure and motion-sensing pad installed on a chair, bed edge, or lift recliner to assess transfers, weight shift quality, and seated pressure exposure.

### Hardware

- **SoC:** ESP32-S3-WROOM-1-N16R8
- **mmWave occupancy radar:** TI IWR6843AOP
- **Pressure matrix:** 16x16 FSR matrix through 4× CD74HC4067 multiplexers and ADS7953 ADC
- **Load cells:** 4× 50 kg corner cells via NAU7802
- **Temp/humidity:** SHT41 for sweat / surface condition context
- **Sub-GHz radio:** SX1262
- **Haptic/audible:** coin vibration motor + buzzer for coached pacing
- **Power:** 12 V wall input with 2-cell LiFePO4 backup option

### Pin assignment

| Signal | ESP32-S3 Pin | Peripheral |
|--------|---------------|-----------|
| I2C SDA/SCL | GPIO8 / GPIO9 | NAU7802, SHT41, DRV2605L |
| SPI MOSI/MISO/SCLK | GPIO11 / GPIO13 / GPIO12 | SX1262, ADS7953 |
| SX1262 CS/BUSY/DIO1/RST | GPIO10 / GPIO14 / GPIO15 / GPIO16 | radio |
| UART1 TX/RX | GPIO17 / GPIO18 | IWR6843AOP |
| MUX select A-D | GPIO4 / GPIO5 / GPIO6 / GPIO7 | FSR matrix mux |
| Buzzer | GPIO38 | local cues |

### Measurement flow

1. Sample pressure map at 20 Hz during occupied state.
2. Sample load cells at 80 Hz during transfers.
3. Run radar posture/occupancy classification at 10 Hz.
4. Compute forward lean, asymmetry, seat unloading rate, and failed-stand attempts locally.
5. Publish summary every 5 s or instantly for unsafe transfers.

---

## Node 4 - Doorway Controller

### Role

Controls an interior low-energy door opener, latch strike, threshold light strip, and clearance sensing so mobility-aid users do not need to fight heavy doors.

### Hardware

- **MCU:** STM32G474RET6
- **Sub-GHz radio:** SX1262
- **UWB anchor:** DW3110
- **Motor driver:** DRV8876 for door opener clutch / arm drive
- **Position sensing:** AS5600 magnetic angle sensor on hinge or drive arm
- **Threshold sensing:** VL53L1X ToF + break-beam pair for obstruction detection
- **Lock/latch:** 12 V electric strike + reed switch feedback
- **Visual cues:** addressable RGB threshold strip
- **Power:** 24 VDC input, 5 V buck, 3.3 V buck, 15 F supercapacitor for close-cycle completion during brownout

### Modes

- approach-open for walker users
- caregiver escort mode
- privacy mode (manual only)
- nighttime silent assist with low-speed movement and dim lighting
- emergency unlock during panic event

---

## Node 5 - Wearable Band

### Role

Lightweight wrist or forearm wearable that provides physiological readiness, fall confirmation, panic alerts, and silent haptic guidance.

### Hardware

- **MCU:** nRF52840
- **PPG/SpO2:** Analog Devices MAX86176
- **IMU:** Bosch BMI270
- **Skin temperature:** TMP117
- **Haptics:** DRV2605L + LRA motor
- **Buttons:** SOS + acknowledge
- **Battery:** 250 mAh LiPo, MCP73831 charger, MAX17048 fuel gauge
- **Radio:** BLE 5.3

### Battery strategy

- 1 Hz low-power HR trend mode when resting
- 50 Hz burst mode during transfers or walking
- opportunistic sync through walker or hub
- target runtime: 36 h typical, 24 h heavy haptic use

---

## Communications Protocol

MobilitySync uses a compact binary protocol named **MSMP** over Sub-GHz TDMA and BLE characteristic payloads.

| Field | Bytes | Notes |
|-------|-------|-------|
| preamble | 1 | 0xC3 |
| version | 1 | protocol version |
| message_type | 1 | telemetry, alert, command, ack, config |
| node_type | 1 | hub, walker, mat, doorway, band |
| source_id | 2 | unique node ID |
| destination_id | 2 | 0xFFFF for broadcast |
| flags | 1 | battery-low, ack-required, encrypted |
| sequence | 1 | wraparound |
| payload_len | 1 | up to 96 bytes |
| payload | 0-96 | packed telemetry |
| crc16 | 2 | CCITT |

### Example payload types

- `TRANSFER_FEATURES`: asymmetry, unload rate, lean angle proxy, retry count
- `WALKER_STATE`: handle forces, wheel speed, brake state, slip score
- `DOOR_EVENT`: approach accepted, obstruction, open complete, manual override
- `BAND_STATUS`: HR, HRV proxy, fall confidence, SOS flag
- `HUB_COMMAND`: open door, engage brakes, haptic cue, rest reminder

---

## Software Stack

### Edge / cloud backend

- FastAPI REST service for provisioning, telemetry ingest, summaries, and reports
- MQTT topic tree for node telemetry and actuation
- SQLite for demo mode, PostgreSQL for production deployment
- model registry hooks for new TFLite / ONNX artifacts
- caregiver notification adapters (SMS/email/push placeholders)

### Mobile app

React Native app includes:

- live home status dashboard;
- transfer history and fatigue heatmap;
- doorway automation settings;
- caregiver shared view;
- device commissioning screens;
- haptic vocabulary configuration for the wearable band.

### ML pipeline

- synthetic dataset generation for prototyping;
- temporal model training for transfer and fatigue risk;
- route / doorway timing prediction;
- recovery trend estimation;
- export scripts to ONNX / TFLite-ready feature contracts.

---

## BOM Overview

Full per-node CSV BOMs live in `hardware/bom/`.

| Node | Approx prototype BOM |
|------|----------------------|
| Mobility Hub | $218 |
| Smart Walker | $286 (excluding mechanical walker frame and battery pack) |
| Transfer Mat | $154 |
| Doorway Controller | $132 (excluding door arm hardware) |
| Wearable Band | $46 |

---

## Repository Layout

```
MobilitySync/
├── README.md
├── schematic/
├── firmware/
├── hardware/bom/
├── software/dashboard/
├── software/ml-pipeline/
├── software/mobile-app/
├── docs/
└── scripts/
```

## Build Notes

- Firmware sources are written in portable C with hardware abstraction placeholders so the packet layer and safety logic can be compiled on a workstation for review.
- Backend can be run locally with `uvicorn main:app --reload` from `software/dashboard/`.
- ML scripts generate synthetic datasets so the full pipeline is runnable without protected clinical data.

## Why People Would Want It

MobilitySync does not just detect a fall after it happens. It helps prevent the fall, reduces embarrassing friction around doors and transfers, gives caregivers confidence, and gives mobility-impaired users more independence at home without adding cameras.
