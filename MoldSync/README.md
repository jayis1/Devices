# MoldSync

**AI-powered whole-home moisture, condensation, and mold prevention system** — a multi-node residential health platform that watches hidden humidity buildup before it becomes visible mold, automatically dries wet rooms, isolates plumbing incidents, and gives families a buildable path to healthier indoor air.

## What It Solves

Millions of homes slowly accumulate moisture behind walls, under sinks, around windows, and inside bathrooms. By the time mold becomes visible, occupants may already be dealing with respiratory irritation, odors, material damage, and expensive remediation.

MoldSync is designed to prevent that progression by closing the loop from **measurement -> prediction -> intervention -> verification**.

- **Bathrooms stay wet too long** after showers, especially in rentals and older homes with weak exhaust fans.
- **Basements and laundry rooms swing above safe dew point** where condensation forms on cold pipes, slab edges, and rim joists.
- **Small plumbing leaks go undetected** because they are intermittent and hidden.
- **Occupants do not know which action matters most**: run fan longer, open window, increase dehumidifier setpoint, shut off a branch line, or call for remediation.
- **Air-quality systems rarely understand mold risk at the surface level**; they see room RH, not cold-wall condensation margin.

MoldSync combines a hub, distributed wall/room sentinels, smart vent/dehumidifier controllers, plumbing interlocks, and a handheld inspection wand. Together they track moisture load, estimate surface condensation risk, detect hidden wetness trends, and automatically intervene before spores amplify.

---

## System Architecture

```text
┌────────────────────────────────────────────────────────────────────────────────────────────┐
│                           MoldSync Cloud / Edge Intelligence Stack                        │
│ FastAPI + MQTT + PostgreSQL + object storage + model registry + Grafana                  │
│                                                                                            │
│ Models                                                                                     │
│ • DewPointNet GRU         - 24 h surface condensation forecast per zone                    │
│ • LeakSignature XGBoost   - hidden leak vs occupancy moisture source classification        │
│ • SporeRisk TFT           - 7 day mold amplification risk timeline                         │
│ • DryingPolicy DQN        - fan / dehumidifier / vent runtime optimization                 │
│ • ThermalMoisture U-Net   - inspection wand damp patch segmentation                        │
│ • OccupancyMoisture RF    - shower / cooking / laundry event attribution                   │
└────────────────────────────────────────────┬───────────────────────────────────────────────┘
                                             │ MQTT over TLS / HTTPS
                                   ┌─────────┴─────────┐
                                   │ MoldSync Hub      │
                                   │ CM4 + RP2040 +    │
                                   │ SX1262 coordinator│
                                   └───────┬─────┬─────┘
                                           │     │
                        Sub-GHz 868 MHz TDMA│     │BLE / Wi-Fi commissioning
                                           │     │
          ┌───────────────────────┬────────┘     └─────────┬─────────────────────────┐
          │                       │                          │                         │
┌─────────┴──────────┐  ┌─────────┴──────────┐   ┌──────────┴──────────┐   ┌──────────┴─────────┐
│ Room Sentinel ×N   │  │ Vent Controller ×M │   │ Plumbing Interlock  │   │ Inspection Wand     │
│ wall temp + RH +   │  │ exhaust fan /      │   │ valve + leak rope + │   │ thermal + NIR +     │
│ VOC + material     │  │ dehumidifier / ERV │   │ pipe temp + flow    │   │ moisture inspection  │
│ moisture trend     │  │ relays             │   │ sensing             │   │                      │
└────────────────────┘  └────────────────────┘   └─────────────────────┘   └────────────────────┘
```

## Node Summary

| Node | Core silicon | Role | Power | Communications |
|------|--------------|------|-------|----------------|
| **MoldSync Hub** | Raspberry Pi CM4 + RP2040 + SX1262 | Local policy engine, MQTT bridge, OTA, dashboard, historical storage, cloud uplink | 12 V / 3 A wall input + dual-18650 UPS | Ethernet, Wi-Fi, BLE 5.0, Sub-GHz 868 MHz |
| **Room Sentinel** | STM32WL55JC | Per-room dew point and hidden wetness sensing, event attribution, mesh endpoint | USB-C 5 V or 2×AA lithium | Sub-GHz 868 MHz |
| **Vent Controller** | ESP32-S3-WROOM-1 + SX1262 | Exhaust fan/dehumidifier/ERV actuation, current verification, local safety logic | 24 VAC or 12 VDC + isolated 5 V | Wi-Fi, BLE, Sub-GHz 868 MHz |
| **Plumbing Interlock** | nRF52840 + SX1262 | Leak rope, flow anomaly detection, pipe freeze monitoring, motorized valve control | 12 VDC + LiFePO4 backup | BLE 5.0, Sub-GHz 868 MHz |
| **Inspection Wand** | ESP32-S3-WROOM-1-N16R8 | Thermal and multispectral handheld inspection with guided remediation workflow | 3000 mAh Li-ion | Wi-Fi, BLE, Sub-GHz 868 MHz |

---

## Daily User Experience

1. A family finishes morning showers.
2. Bathroom **Room Sentinel** detects RH climbing to 88%, wall temperature at the exterior tile face 3.1 °C below room air, and a condensation margin approaching zero.
3. The **Vent Controller** automatically extends exhaust runtime for 19 minutes, then stages a dehumidifier only if the room still fails to recover below target.
4. In the basement, another sentinel sees nightly humidity rebounds plus a cold-rim-joist signature inconsistent with occupancy. The **LeakSignature** model raises a hidden-leak suspicion score.
5. The **Plumbing Interlock** confirms low but continuous flow overnight and sends an amber alert for the humidifier branch line.
6. The app guides the user to scan the suspect wall with the **Inspection Wand**, which highlights a damp patch behind shelving.
7. The hub recommends: isolate humidifier line, run utility-room dehumidifier to 47% RH, inspect sill plate, and schedule remediation if dampness persists beyond 48 hours.
8. Weekly, the hub publishes a home **Spore Risk Score**, drying efficiency trend, and rooms needing attention.

---

## Node 1 - MoldSync Hub

### Core hardware

- **Compute module:** Raspberry Pi Compute Module 4, 4 GB RAM, 32 GB eMMC
- **Supervisory MCU:** RP2040 for watchdog, slot timing, radio failover, power sequencing
- **Sub-GHz radio:** Semtech SX1262 at 868 MHz
- **Storage:** eMMC + mirrored industrial microSD
- **UI:** 7 inch capacitive HDMI/DSI display, tower RGB LED, mute switch, piezo buzzer
- **Backup:** dual 18650 UPS HAT for >2 h outage runtime

### Responsibilities

- Maintains room models, safe RH envelopes, and material-specific condensation policies
- Bridges all node telemetry to MQTT and REST APIs
- Stores remediation plans, occupancy schedules, and building metadata (window type, insulation assumptions, HVAC equipment)
- Executes local control when internet is unavailable
- Generates **Mold Risk**, **Drying Efficiency**, **Hidden Leak Suspicion**, and **Condensation Margin** scores

### Key interfaces

| Interface | Connected device |
|-----------|------------------|
| RP2040 SPI0 | SX1262 radio |
| RP2040 I2C0 | INA219, DS3231 RTC, buzzer GPIO expander |
| RP2040 UART0 | CM4 heartbeat channel |
| CM4 Ethernet | primary uplink |
| CM4 USB2 | service / export |
| CM4 DSI/HDMI | local touchscreen |

### Power architecture

- 12 V input enters TVS + resettable fuse stage
- LM2596S-5.0 generates 5 V rail for CM4 and display
- TPS62172 creates 3.3 V logic rail
- UPS charger manages dual protected 18650 cells
- RP2040 monitors battery current and safe shutdown threshold

---

## Node 2 - Room Sentinel

### Hardware architecture

- **MCU/radio:** STM32WL55JC (Cortex-M4 + integrated Sub-GHz)
- **Temp/RH:** Sensirion SHT45 placed in ventilated chimney
- **CO2:** Sensirion SCD41 for occupancy differentiation in closed rooms
- **VOC:** Bosch BME688 for moisture-source context and musty-event trend features
- **Surface temperature:** Melexis MLX90614ESF aimed at wall/window surface
- **Material moisture:** TI FDC2214 capacitance front-end driving two adhesive-backed copper foil electrodes or PCB interdigitated pads
- **Ambient lux:** VEML7700 for solar gain / condensation context
- **User feedback:** RGB LED + single service button

### Why these parts

- **STM32WL55** avoids an external radio and supports deterministic TDMA scheduling.
- **SHT45 + MLX90614** provide the core air-to-surface condensation margin.
- **FDC2214** allows non-invasive hidden wetness trend sensing through drywall or cabinet back panels when properly calibrated.
- **SCD41** helps separate human moisture events from plumbing or envelope issues.

### Pin assignment

| Signal | STM32WL55 Pin | Peripheral |
|--------|---------------|------------|
| I2C2 SDA/SCL | PB11 / PB13 | SHT45, SCD41, BME688, FDC2214, VEML7700 |
| I2C1 SDA/SCL | PB7 / PB6 | MLX90614 |
| GPIO EXTI | PA0 | service button |
| GPIO | PA8 | RGB LED data |
| ADC1 IN5 | PA0_ALT | battery sense |
| USART1 TX/RX | PA9 / PA10 | debug header |

### Measurement flow

1. Sample air temp/RH/CO2/VOC every 30 s.
2. Sample surface temperature every 15 s in bathrooms and window zones.
3. Sample capacitive moisture baseline every 5 min.
4. Compute dew point, surface margin, and moisture anomaly features locally.
5. Send compressed telemetry frame to hub every 60 s, or immediately on threshold crossing.

---

## Node 3 - Vent Controller

### Role

Drives exhaust fans, inline boosters, ERV/HRV boost contacts, and dehumidifiers while verifying the commanded equipment actually ran.

### Hardware

- **SoC:** ESP32-S3-WROOM-1
- **Sub-GHz radio:** SX1262 on SPI
- **Relay outputs:** 4× Omron G5Q isolated relays or SSRs depending load type
- **Current verification:** ACS37800 energy monitor on switched load line
- **Room pressure feedback:** SDP31 differential pressure sensor for bath fan verification
- **Temp/RH local backup:** SHT41
- **Safety:** opto-isolated inputs, MOV surge suppression, fused mains daughtercard
- **UX:** e-paper status strip, override button, commissioning BLE

### Pin assignment

| Signal | ESP32-S3 Pin | Peripheral |
|--------|---------------|-----------|
| I2C SDA/SCL | GPIO8 / GPIO9 | ACS37800, SDP31, SHT41 |
| SPI MOSI/MISO/SCLK | GPIO11 / GPIO13 / GPIO12 | SX1262 |
| SX1262 CS/BUSY/DIO1/RST | GPIO10 / GPIO14 / GPIO15 / GPIO16 | radio |
| Relay 1..4 | GPIO4 / GPIO5 / GPIO6 / GPIO7 | fan / dehumidifier / ERV / spare |
| Override button | GPIO17 | user input |
| E-paper CS/DC/RST | GPIO35 / GPIO36 / GPIO37 | status display |

### Control modes

- Shower dry-out extension
- Laundry moisture purge
- Basement humidity clamp
- Window anti-condensation overnight purge
- Remediation mode with continuous or duty-cycled drying

---

## Node 4 - Plumbing Interlock

### Role

A near-plumbing safety node that combines leak rope, pipe temperature, branch flow observation, and motorized shutoff control.

### Hardware

- **MCU:** nRF52840
- **Long-range radio:** SX1262
- **Valve drive:** DRV8871 H-bridge for 12 V motorized ball valve
- **Flow meter:** YF-S201 pulse or industrial Hall turbine input through opto-isolation
- **Leak detection:** 3-zone resistive leak rope + ADS1115 ADC multiplexing
- **Pipe temperature:** 2× TMP117 (cold line and room reference)
- **Current sense:** INA219 for valve motor confirmation
- **Backup power:** 4-cell LiFePO4 pack + charger for fail-closed operation
- **UX:** local acknowledge button, piezo buzzer, tri-color status LED

### Logic

- Detect continuous overnight low-flow signatures
- Freeze-risk protection based on pipe-vs-room delta and exterior weather input from cloud
- Auto-close valve only on high-confidence leak events or user-confirmed anomalies
- Continue beaconing alarms over Sub-GHz even if Wi-Fi is unavailable

---

## Node 5 - Inspection Wand

### Role

A handheld troubleshooting tool for targeted wall, ceiling, cabinet, and window inspections when the system wants visual confirmation.

### Hardware

- **SoC:** ESP32-S3-WROOM-1-N16R8
- **Thermal imager:** MLX90640 32×24 array
- **RGB camera:** OV2640
- **Spectral channels:** AS7341 + 850 nm IR LED + UV-A LED for fluorescence clues on microbial growth / prior staining
- **Moisture contact mode:** spring probes + AC-excited conductivity measurement through AD5933
- **Distance:** VL53L1X for consistent standoff
- **UI:** 2.8 inch TFT, haptic motor, trigger switch, speaker
- **Battery:** single-cell 3000 mAh Li-ion with USB-C charging

### Guided inspection flow

1. User receives target room and suspected surface from app.
2. Wand walks the user to keep 20-30 cm standoff.
3. Thermal mosaic and spectral channels are captured.
4. Edge model segments likely damp region.
5. Hub compares result with historical room telemetry and suggests next step.

---

## Communication Protocol

All nodes use a shared MoldSync Protocol over **Sub-GHz 868 MHz TDMA** with BLE commissioning on mobile-facing nodes.

### Frame fields

| Field | Bytes | Notes |
|------|-------|------|
| preamble | 1 | 0xB7 |
| version | 1 | currently 0x01 |
| message_type | 1 | telemetry / event / command / ack / config / OTA |
| node_type | 1 | hub / sentinel / vent / plumbing / wand |
| source_id | 2 | unique node ID |
| destination_id | 2 | hub or endpoint |
| flags | 1 | ack requested, encrypted, urgent |
| sequence | 1 | wraps at 255 |
| payload_length | 1 | max 64 |
| payload | 0-64 | CBOR-like packed fields |
| crc16 | 2 | CCITT |

### Topic examples

- `moldsync/home/telemetry/room-sentinel/0214`
- `moldsync/home/event/plumbing/0031`
- `moldsync/home/command/vent/0017`
- `moldsync/home/alert/condensation/bathroom-east`

---

## ML Pipeline Overview

### 1. DewPointNet GRU
Forecasts room RH, dew point, and surface margin 24 hours ahead using weather, occupancy, HVAC runtime, and prior moisture history.

### 2. LeakSignature XGBoost
Classifies anomalies as **plumbing leak**, **shower moisture**, **cooking moisture**, **laundry moisture**, or **envelope infiltration**.

### 3. SporeRisk Temporal Fusion Transformer
Produces a 7 day room-level mold amplification risk score from wetness duration, predicted margin violations, remediation history, and VOC shifts.

### 4. DryingPolicy DQN
Learns energy-aware fan and dehumidifier runtimes that minimize damp hours while limiting kWh and noise burden.

### 5. ThermalMoisture U-Net
Segments damp/cold patches in inspection-wand imagery and returns remediation confidence.

### 6. OccupancyMoisture Random Forest
Separates occupant-generated humidity spikes from hidden moisture sources when only sparse sensing is available.

---

## Buildable BOM Summary

Detailed BOM CSV files live in `hardware/bom/`. Major production-intent parts:

- Raspberry Pi CM4, RP2040, SX1262, LM2596S, TPS62172
- STM32WL55, SHT45, SCD41, BME688, FDC2214, MLX90614, VEML7700
- ESP32-S3, ACS37800, SDP31, Omron G5Q relays, SHT41
- nRF52840, DRV8871, TMP117, ADS1115, INA219, motorized ball valve
- ESP32-S3, MLX90640, OV2640, AS7341, AD5933, VL53L1X

---

## Repository Layout

```text
MoldSync/
├── README.md
├── schematic/
│   ├── hub/hub.sch
│   ├── room-sentinel/room-sentinel.sch
│   ├── vent-controller/vent-controller.sch
│   ├── plumbing-interlock/plumbing-interlock.sch
│   └── inspection-wand/inspection-wand.sch
├── firmware/
│   ├── common/
│   ├── hub/
│   ├── room-sentinel/
│   ├── vent-controller/
│   ├── plumbing-interlock/
│   └── inspection-wand/
├── hardware/bom/
├── software/
│   ├── dashboard/
│   ├── ml-pipeline/
│   └── mobile-app/
├── docs/
└── scripts/
```

## Safety and deployment notes

- Mains switching for fans/dehumidifiers must use locally compliant creepage/clearance and enclosure design.
- Leak-triggered valve closure should never isolate life-safety fire sprinklers or required medical water loads.
- MoldSync is a prevention and prioritization system, not a substitute for licensed remediation when contamination is extensive.

## Next build steps

1. Fabricate room sentinel and vent controller boards.
2. Calibrate material-specific capacitance baselines with the included script.
3. Train room models on 2-4 weeks of home telemetry.
4. Enable auto-intervention only after a burn-in period and user review.
