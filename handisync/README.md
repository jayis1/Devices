# HandiSync — Daily-Living Accessibility and Assistive Control

HandiSync gives people with limited hand strength, dexterity, tremor, or fatigue an independent way to prepare food, operate essential appliances, open drawers, and ask for help. It is a local-first, multi-node system: wearable grips quantify effort and tremor, a privacy-preserving voice/gesture sentinel recognizes a small enrolled command vocabulary, appliance docks safely actuate approved loads, and powered drawer/door actuators remove a frequent physical barrier. The hub coordinates the system even when the internet is down.

> **Safety boundary:** HandiSync is assistive equipment, not a medical device. Appliance power is only enabled by a locally armed hardware switch, occupancy/current/temperature checks, and a timed fail-off. It must not control heaters, cooking appliances, locks, or medical equipment without certified, jurisdiction-appropriate hardware and professional installation.

## System at a glance

| Node | Compute | Job | Link | Power |
|---|---|---|---|---|
| Assist Hub | ESP32-S3-WROOM-1 | local policy, dashboard, MQTT bridge, e-ink status | Wi-Fi, SX1262 868/915 MHz | USB-C 5 V + LiFePO4 UPS |
| Smart Grip ×N | nRF52840 | grip force, tremor/gesture IMU, haptic confirmation | BLE 5.3 to hub | 500 mAh LiPo |
| Voice & Gesture Sentinel ×N | ESP32-S3 | on-device wake-word/intent and mmWave pointing gesture | Sub-GHz TDMA | USB-C 5 V |
| Appliance Dock ×M | ESP32-C6 | approved-load relay, current/temperature validation | Sub-GHz TDMA | isolated 5 V PSU |
| Access Actuator ×M | STM32G0B1 | drawer/door linear actuator, force-limited motion | Sub-GHz TDMA | 12 V fused supply |

```text
 Grip (BLE) ──┐                       ┌─ Appliance Dock → approved appliance
 Grip (BLE) ──┼── Assist Hub ──TDMA───┼─ Access Actuator → drawer/cabinet
              │    Wi-Fi/MQTT         └─ Voice & Gesture Sentinel
              └── FastAPI + mobile app + encrypted local event store
```

## What it does

1. A person says a locally enrolled phrase (for example, “open tea drawer”), then points at a registered zone. The Sentinel emits an **intent candidate**, never raw audio.
2. Hub policy requires a zone match, allowed schedule, presence confirmation, and—where configured—grip haptic confirmation before sending an actuation command.
3. The actuator runs until its Hall/limit feedback, current limit, or timeout. It sends a signed outcome.
4. Docks require their physical ARM switch and no fault current/over-temperature condition. Every power session has a maximum duration and local emergency-off.
5. Grip data builds a private effort/tremor trend. The app can suggest lower-force routines or share an export only when the owner chooses.

## Hardware and electrical design

### Assist Hub (ESP32-S3-WROOM-1)
- **Power:** USB-C 5 V → TPS63070 power-path/3.3 V rail; protected 6.6 V LiFePO4 UPS input through TPS25947 eFuse. 2 A resettable fuse. Never backfeed USB.
- **Radios:** SX1262 DIO1 GPIO4, NSS GPIO10, SCK/MISO/MOSI GPIO12/13/11, BUSY GPIO5, RESET GPIO6; antenna keep-out 15 mm. Wi-Fi uses the S3 module antenna.
- **Human interface:** 2.9-in Waveshare e-paper SPI (CS GPIO15, DC16, RST17, BUSY18); guarded emergency-stop input GPIO21 with pull-up and 100 nF debounce.
- **Interfaces:** grip BLE central; I²C GPIO8/9 for ATECC608B secure element and SHTC3 enclosure sensor; UART1 GPIO43/44 optional LTE modem.

### Smart Grip (nRF52840)
- **Power:** MCP73831 LiPo charger, AP2112K-3.3 regulator, MAX17048 fuel gauge (I²C). Battery NTC is read on AIN2.
- **Sensors:** FSR 0–20 kg in divider to SAADC AIN0; ICM-42688-P IMU I²C P0.26/27, interrupt P0.11. DRV2605L haptic driver on I²C.
- **Safety/UX:** physical SOS button P0.13, RGB LED P0.14/15/16. The grip only sends coarse features by default; raw 200 Hz samples are opt-in diagnostic data.

### Voice & Gesture Sentinel (ESP32-S3)
- **Audio:** INMP441 I²S: WS GPIO4, SCK5, SD6. A hardware microphone disconnect slide switch is in series with the 3.3 V mic rail.
- **Gesture:** TI IWRL6432BOOST UART at GPIO17/18; configured for zone/presence vectors, no imaging.
- **Radio:** SX1262 same SPI mapping as hub (GPIO10–13; DIO1 GPIO7). LED GPIO48 indicates microphone state.

### Appliance Dock (ESP32-C6)
- **Mains:** isolated Mean Well IRM-05-5 supply, 1 A time-delay fuse, MOV-14D471K, thermal fuse, and 16 A certified relay/contactor. Creepage/clearance ≥8 mm across reinforced isolation; mains area is segregated.
- **Feedback:** SCT-013-000 CT → burden/biased ADC GPIO0; TMP117 I²C GPIO6/7; ARM key switch GPIO3; physical OFF button GPIO2.
- **Output:** relay driver is opto-isolated; default relay coil state is OFF. Firmware cannot override hardware OFF or ARM.

### Access Actuator (STM32G0B1)
- **Power/motion:** 12 V input → 5 A blade fuse → reverse-polarity MOSFET → 12 V actuator. DRV8876 H-bridge IN1 PA8/IN2 PA9, current sense PA0. Two NC end stops PB0/PB1 and Hall pulse PB2.
- **Control:** SX1262 SPI1 PA5/6/7, NSS PA4, DIO1 PB5. Load cell HX711 PB10/11 confirms a drawer is not obstructed. A red mushroom E-stop physically opens motor supply.

Detailed component designators and procurement quantities are in `hardware/bom/`; build-level wiring and validation is in `docs/architecture.md`.

## Communications and security

- **Grip → hub:** BLE GATT, LE Secure Connections, bonded devices. Characteristics: features (`0xA101`), confirmation (`0xA102`), SOS (`0xA103`).
- **Fixed nodes:** 868 MHz EU / 915 MHz US SX1262 TDMA star. 10 s superframe: hub beacon at slot 0, telemetry slots 1–7, ACK/command slots 8–9. Region selection is compile-time and must match local spectrum rules.
- **Frame:** 32-bit node ID, 32-bit monotonic sequence, type, payload length, payload ≤80 B, CRC-32C; command frames carry HMAC-SHA256 truncated to 16 B. Replay windows reject stale sequences. See `docs/protocol.md`.
- **Cloud:** MQTT over TLS 1.3; FastAPI authenticates per-home tokens. Hub spools encrypted events locally during WAN loss. Local safety rules never depend on cloud availability.

## ML pipeline

| Model | Input | Output | Deployment |
|---|---|---|---|
| GripNet | 2 s IMU + FSR features | stable / high effort / tremor / drop risk | TensorFlow Lite Micro on Grip |
| IntentNet | 1 s Mel spectrogram after wake word | enrolled command class or reject | Sentinel, no transcription |
| ReachRisk | actuator current, duration, outcomes | obstruction / maintenance warning | Hub |
| RoutineForecaster | consented daily completion events | likely missed task window | FastAPI batch job |

Models are decision support, with explicit confidence thresholds, reject classes, demographic/assistive-device evaluation, and human confirmation for actuation. `software/ml-pipeline/train_gripnet.py` is a reproducible baseline trainer; never train on voice unless explicit participant consent and retention limits are documented.

## Repository map

```
firmware/          C sources for hub, grip, sentinel, dock, actuator, shared protocol
schematic/         KiCad 7 schematic source and node connection notes
hardware/bom/      manufacturer-part BOM CSVs
software/dashboard FastAPI/MQTT service and container setup
software/ml-pipeline baseline training/evaluation
software/mobile-app React Native starter
scripts/           deployment and calibration tools
docs/              architecture, API, protocol and safety notes
```

## Build and commissioning

1. Review `docs/architecture.md`, local electrical code, and the appliance manual. Keep mains work with a qualified installer.
2. Assemble and test each low-voltage node on a current-limited bench supply. Run `python3 scripts/calibrate.py --node grip --port /dev/ttyACM0` for each grip.
3. Flash node firmware using its vendor toolchain; provision unique node ID and 32-byte key outside source control.
4. Start backend: `cd software/dashboard && docker compose up --build`. Set `HANDISYNC_MQTT_URL` and `HANDISYNC_API_TOKEN`.
5. Pair grips, map voice zones, set explicit per-dock limits, and test emergency-off/limit-switch behavior with appliance unplugged before any live load.

## Limits and responsible use

Do not infer diagnosis from tremor, fatigue, or routine data. Do not use the system as a sole emergency service. Configure caregiver notifications only with consent. Preserve a clear manual path for every drawer, appliance, and emergency stop.
