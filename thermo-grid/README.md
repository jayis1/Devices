# ThermoGrid

**AI-powered multi-node home thermal comfort & energy optimization system.** Keeps every room at the perfect temperature — predictively, efficiently, and personalized to each person's body — while cutting your energy bill by 20-40%.

---

## What It Does

ThermoGrid is a 4-node system that replaces your dumb thermostat with a whole-home thermal intelligence network:

1. **Senses** the real thermal state of every room — air temperature, radiant wall/floor temperature (MLX90640 thermal IR array), humidity, air velocity, barometric pressure, occupancy, and metabolic heat — not just a single thermostat point in a hallway
2. **Predicts** where heat will go — a physics-informed thermal model learns your home's heat capacity, insulation, solar gain, and inter-room airflow to forecast room temperatures 2-4 hours ahead
3. **Actuates** per-zone heating and cooling — motorized radiator valves, damper controllers for forced-air systems, and relay outputs for radiant floor zones — only conditioning rooms that are occupied or will be soon
4. **Personalizes** comfort — a wearable comfort tag reads your skin temperature, heart rate, and activity; the system learns your individual thermal comfort profile (some people run cold, some hot) and adjusts zones around *you*, not a setpoint on a wall
5. **Optimizes** energy — learns your utility time-of-use schedule, pre-heats during cheap hours (using thermal mass as a battery), avoids conditioning empty rooms, and coordinates with solar generation to use your own power when the sun shines
6. **Saves** money — 20-40% reduction in heating/cooling bills by eliminating whole-house conditioning of unoccupied zones, reducing overheating/overcooling, and shifting loads to off-peak hours
7. **Adapts** — continuous reinforcement-learning comfort model that adapts to seasonal changes, new furniture, opened windows (detected via rapid temp drop + air velocity), and evolving household routines

All room sensors communicate over a dedicated Sub-GHz mesh network (reliable, no WiFi dependency for climate-critical control). The hub bridges to WiFi for cloud analytics and the mobile app. The comfort tag uses BLE. The zone actuator uses both Sub-GHz (to the hub) and direct wired control of radiator valves / dampers / relays.

### The Problem It Solves

- **Whole-house heating/cooling of empty rooms:** Traditional thermostats heat or cool the *entire house* based on one hallway sensor. If you're in the living room at 8pm, why is the bedroom being heated? ThermoGrid conditions only occupied zones (and zones you'll enter soon, predicted by routine learning).
- **Thermostat wars:** One person is cold, another is hot. The central thermostat can't make both happy. ThermoGrid's per-zone control + personal comfort profiles means each person gets *their* temperature in *their* zone.
- **Energy waste from overheating/overcooling:** Most homes overshoot their setpoint by 1-2°C because a single sensor can't represent the whole house. ThermoGrid's distributed sensing eliminates overshoot — every room hits its target precisely.
- **Time-of-use bill waste:** Heating when power is expensive (peak hours). ThermoGrid pre-heats during off-peak using your home's thermal mass as a battery, then coasts through expensive peak hours.
- **No solar coordination:** Homes with solar panels waste generated power exporting it to the grid at low rates. ThermoGrid ramps heating/cooling to consume your own solar production in real time.
- **Cold mornings / hot arrivals:** You wake up to a cold house or come home to a stuffy one. ThermoGrid predicts when you'll wake/arrive and pre-conditions based on learned routines + weather forecast.
- **Drafts and thermal discomfort:** Feeling cold even when the thermostat says 21°C, because the walls are cold (radiant asymmetry). ThermoGrid measures *mean radiant temperature* (MRT) via thermal IR arrays, not just air temperature, and conditions for actual comfort.
- **Radiator valve manual fiddling:** TRVs are set once and forgotten, or constantly fiddled. ThermoGrid's smart radiator valves are autonomous — each room finds its own optimum.

ThermoGrid senses, predicts, actuates, and personalizes — so every room is at *your* perfect temperature, and your energy bill drops by a third.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                         THERMOGRID SYSTEM                                     │
│                                                                               │
│  ┌───────────────────┐   Sub-GHz    ┌───────────────────┐                     │
│  │ COMFORT TAG        │◄─── BLE ───►│                   │                     │
│  │ (worn on wrist/    │             │   HUB NODE        │──── WiFi6 ───►Cloud │
│  │  chest/clip)       │             │  (RP2040 +        │               Dashboard
│  │ nRF52840 + skin    │             │   ESP32-C6)       │               + ML    │
│  │ temp + HR + accel  │             │                   │               Pipeline│
│  │ 8-12 months coin   │             │  Edge ML:         │               + Solar │
│  └───────────────────┘             │  thermal forecast │               + Weather│
│                                     │  comfort model    │               + Alerts│
│  ┌───────────────────┐              │  TFT: thermal map │─── BLE ──────► Mobile │
│  │ ROOM SENSOR ×N     │◄───────────►│  Siren: n/a       │                 (React │
│  │ (one per room)      │  915MHz LoRa │  Solar coord      │                 Native)│
│  │ STM32WL55 + MLX90640│  (mesh)     │  TOU optimizer    │                     │
│  │ + SHT45 + air vel  │              │                   │                     │
│  │ + occupancy PIR    │              └─────────┬─────────┘                     │
│  │ Solar + AA         │                         │ Sub-GHz                       │
│  └───────────────────┘                         ▼                               │
│                                     ┌───────────────────┐                      │
│                                     │ ZONE ACTUATOR ×M   │                      │
│                                     │ (per heating zone)│                      │
│                                     │ ESP32-C3 + motor   │                      │
│                                     │ radiator valves    │                      │
│                                     │ + damper + relay   │                      │
│                                     │ 24VAC or solar+batt│                      │
│                                     └───────────────────┘                      │
│                                                                               │
│  ┌───────────────────────────────────────────────────────────────────────┐   │
│  │                    CLOUD / EDGE SOFTWARE                              │   │
│  │  ┌─────────┐  ┌──────────────┐  ┌───────────────────────┐             │   │
│  │  │Dashboard│  │ ML Pipeline  │  │ Mobile App            │             │   │
│  │  │ (React) │  │ Thermal forecast│  │ Live thermal map     │             │   │
│  │  │ Energy  │  │ Comfort model │  │ Per-zone setpoints    │             │   │
│  │  │ Savings │  │ Routine learn │  │ "I'm cold" → boost    │             │   │
│  │  │ Zones   │  │ Solar coord   │  │ Schedule + routines  │             │   │
│  │  └─────────┘  └──────────────┘  └───────────────────────┘             │   │
│  └───────────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Nodes

### 1. Hub Node (1 per system)

The brain. Bridges the Sub-GHz mesh to WiFi/BLE/cloud. Runs the thermal forecast model, comfort optimization, and solar/TOU coordination.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | RP2040 + ESP32-C6 | RP2040 runs mesh + thermal ML + display; ESP32-C6 handles WiFi/BLE |
| Radio | SX1262 (868/915MHz) | Sub-GHz LoRa mesh to all room sensors + zone actuators (+20dBm) |
| Display | 3.2" IPS TFT (ILI9488) | Home thermal map: per-room temp, occupancy, zone states, energy |
| Storage | W25Q256 32MB Flash + MicroSD | Thermal model cache, event log, OTA |
| RTC | PCF8563 + CR1220 | Timekeeping for schedules + TOU even without WiFi |
| Power | 5V USB-C + LiPo 2500mAh backup | Stays running during power outage (climate safety!) |
| Connectors | 4× I2C, 2× UART, 8× GPIO | Expansion (outdoor temp sensor, weather station) |
| LEDs | RGB status LED | System state: running/optimizing/saving |

**Hub firmware responsibilities:**
- Mesh network coordinator (TDMA scheduler for all sensors + actuators)
- Thermal forecast engine: physics-informed RC-network model of the home, predicts room temps 2-4h ahead
- Comfort optimizer: per-person comfort profile + per-zone setpoint optimization (MILP or heuristic)
- Solar coordination: queries inverter/gateway for current solar production, ramps heating/cooling to self-consume
- TOU optimization: knows your electricity tariff schedule, pre-heats during cheap hours using thermal mass
- Routine learning: learns when each room is typically occupied (morning bedroom, evening living room) and pre-conditions ahead
- WiFi uplink to MQTT broker (QoS 1, TLS) + telemetry to cloud
- BLE GATT server for mobile app (status, override, "I'm cold/hot")
- TFT dashboard rendering (home thermal map, energy bar, solar gauge, zone states)
- OTA update distribution to all nodes
- TFLite Micro inference: thermal forecast, comfort prediction

### 2. Room Sensor Node (1 per room, N total)

The senses. One per room (living room, bedroom, kitchen, bathroom, office, etc.). Measures the *real* thermal environment including radiant temperature.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32WL55JC (Cortex-M4 + Sub-GHz radio in one chip) | Ultra-low-power, integrated LoRa — no separate radio chip needed |
| Thermal IR | MLX90640 (16×12 thermal IR array) | Mean radiant temperature: measures wall/floor/window surface temps |
| Temp/Humidity | SHT45 (±0.1°C, ±1.5%RH) | Air temperature + humidity (the accurate reference) |
| Air velocity | SDP810 (differential pressure) | Detects drafts, airflow, open windows (rapid temp drop + air movement) |
| Pressure | BMP390 | Barometric pressure (weather correlation, stack effect) |
| Occupancy | AM612 PIR + HLK-LD2410B mmWave | Presence detection (mmWave distinguishes person vs pet, works in dark) |
| Light | ALS-PT19 | Ambient light (solar gain correlation, day/night) |
| CO2 (optional) | SCD41 (photoacoustic) | CO2 — correlates with occupancy + ventilation needs |
| Power | 2× AA + 0.5W solar panel + MCP73831 | 12+ months battery, solar trickle charges |
| Enclosure | IP-rated indoor, vented | Wall-mount, allows airflow to sensors |

**Room sensor firmware (battery-critical + accurate):**
- Deep sleep <8µA, wakes every 30s (configurable 10s-5min) for measurement
- Thermal IR scan: MLX90640 reads 192 pixels of wall/floor/window surface temps → compute mean radiant temperature (MRT)
- Air temp + humidity: SHT45 (the reference sensor, ±0.1°C accuracy)
- Air velocity: SDP810 differential pressure → detect drafts, HVAC airflow, open windows
- Occupancy: mmWave presence (sub-meter, works through furniture) + PIR (fast wakeup)
- Open-window detection: rapid temp drop + air velocity spike + humidity change → WINDOW_OPEN event (hub pauses conditioning for that zone)
- Solar gain estimation: light sensor + MRT of sunlit wall/window → estimate BTU input from sun
- Reports to hub over LoRa SF7 every 30s (telemetry) or immediately on WINDOW_OPEN / occupancy change
- Solar-aware: faster poll during daylight (more dynamic), slower at night (stable)

### 3. Zone Actuator Node (1 per heating/cooling zone, M total)

The hands. Controls one heating/cooling zone — motorized radiator valves, HVAC dampers, or relay outputs for radiant floor / electric zones.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | ESP32-C3 (RISC-V, WiFi + Sub-GHz via SX1261) | Zone controller + Sub-GHz mesh + optional WiFi for direct cloud fallback |
| Radio | SX1261 (868/915MHz) | Sub-GHz mesh client to hub |
| Radiator valve | Danfoss RA2 motorized valve + M30×1.5 actuator (or equivalent) | Per-room radiator control (EU homes) |
| Damper control | Servo (MG996R) + 24VAC damper linkage | Forced-air zone damper (US homes) |
| Relay outputs | 2× relay + optocoupler (SRD-05VDC) | Radiant floor zone valve, electric baseboard, boiler relay, heat pump zone |
| Temperature | DS18B20 (zone pipe/floor temperature) | Feedback for valve/damper control loop (PID) |
| Flow sensor | YF-S201 flow meter (optional, hydronic systems) | Measure actual water flow for per-zone energy accounting |
| Power | 24VAC (from boiler/transformer) or 4× AA + boost | Wired zones use 24VAC; wireless zones use AA with boost converter |
| Enclosure | DIN-rail or junction box | Near radiator / air handler / manifold |

**Zone actuator firmware (real-time control):**
- PID control loop: reads zone pipe temperature (DS18B20) + receives zone setpoint from hub → drives valve/damper/relay
- Motorized radiator valve: PWM-positioned actuator (0-100% open), PID on room temp feedback from corresponding room sensor
- Damper control: servo position 0-90° modulates airflow to zone
- Relay mode: bang-bang with hysteresis (±0.5°C) for on/off zone valves / electric heat
- Energy accounting: flow sensor + pipe temp delta → BTU per zone (hydronic systems)
- Failsafe: if hub loses contact for >10min, reverts to last setpoint (frozen) or frost-protection mode (5°C minimum)
- Reports valve position, flow, energy, and faults to hub every 30s
- Receives setpoint updates from hub over Sub-GHz (every 30s or on change)

### 4. Comfort Tag (1 per person, worn)

The personalizer. A small wearable that reads your skin temperature, heart rate, and activity to learn your individual thermal comfort profile.

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | nRF52840 (Cortex-M4F, BLE 5.0) | BLE to phone/hub + sensor management |
| Skin temp | MAX30208 (clinical-grade ±0.1°C) | Wrist/skin temperature — key comfort input |
| Air temp | TMP117 (±0.1°C) | Ambient near body |
| HR/HRV | MAX30101 (PPG) or BHI260 (smart sensor hub) | Heart rate + HRV — metabolic heat proxy |
| Accel/Gyro | LSM6DSO (6-axis IMU) | Activity level (sedentary/active/exercising) — changes thermal need |
| Humidity | SHT40 (tiny) | Local humidity near skin (sweat evaporation comfort) |
| Power | CR2032 or small LiPo + charging coil | 8-12 months on CR2032; rechargeable LiPo option with Qi |
| Enclosure | Watch-band clip / lanyard / pin | Wearable form factor |
| BLE | 5.0 (built-in nRF52840) | To hub (when in range) or phone (relay to cloud) |

**Comfort tag firmware (ultra-low-power):**
- BLE peripheral: advertises every 2s, connects to hub or phone
- Samples: skin temp (MAX30208) + air temp (TMP117) every 30s, HR (MAX30101) every 2min, accel every 1min
- Activity classification: IMU-based (sedentary/light/moderate/vigorous) — determines metabolic heat
- Comfort votes: user presses button when uncomfortable → "I'm cold" / "I'm hot" → tagged with current sensor readings → trains personal comfort model
- Comfort inference: on-device lightweight model predicts comfort score from skin temp, air temp, HR, activity
- Sleep: <15µA between samples, ~5mA for 50ms during measurement burst
- Battery: 8-12 months on CR2032 at 30s/2min sampling intervals
- Fallback: if no BLE connection for >1hr, reduces to 5min sampling to save battery

---

## Communication Protocol

### Sub-GHz Mesh (SX1262/61/STM32WL55, 868/915MHz LoRa)

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa SF7 (normal telemetry) / SF9 (actuator commands) / SF12 (emergency freeze alert) |
| Bandwidth | 125 kHz |
| TX Power | +14 dBm (sensors) / +20 dBm (hub) |
| Range | 30m indoor (normal) / 100m (SF9) / 300m+ (SF12) |
| Protocol | Custom TDMA (hub is coordinator) |
| Slot Duration | 100ms per node |
| Cycle Time | Dynamic: (N+2)×100ms where N = number of room sensors |

### TDMA Frame Structure

```
│ SLOT 0   │ SLOT 1      │ SLOT 2      │ ... │ SLOT N+1     │ SLOT N+2  │
│ HUB CMD  │ SENSOR 1    │ SENSOR 2    │ ... │ ACTUATORS    │ CTRL/ACK  │
│ 100ms    │ 100ms       │ 100ms       │     │ 100ms (all)  │ 100ms    │
│
Slot 0: Hub broadcasts sync + zone setpoints + armed state
Slots 1..N: Each room sensor uplinks (temp, MRT, humidity, occupancy, etc.)
Slot N+1: Zone actuators uplink (valve pos, flow, energy, faults)
Slot N+2: Control / ACK / retransmit / OTA

Freeze Protection Override:
  If any room sensor reports <4°C, hub immediately broadcasts
  FREEZE_ALERT on SF12. All zone actuators open valves to 100%.
  Boiler/heat-pump relay forced ON. Prevents pipe damage even
  if WiFi/cloud is down.

Window-Open Override:
  If room sensor detects WINDOW_OPEN (rapid temp drop + air velocity),
  hub immediately sends VALVE_CLOSE to that zone's actuator.
  No energy wasted heating an open room.
```

### Mesh Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2) ]

TYPE values:
  0x01 = SENSOR_DATA (air_temp, MRT, humidity, air_vel, pressure, occupancy, light, CO2)
  0x02 = ACTUATOR_DATA (valve_pos, flow, energy, pipe_temp, fault, battery)
  0x03 = COMFORT_DATA (skin_temp, air_temp, HR, HRV, activity, comfort_vote)
  0x04 = COMMAND (setpoint, valve_pos, damper_pos, relay_on, mode)
  0x05 = ACK
  0x06 = OTA_BLOCK
  0x07 = CALIBRATION
  0x08 = FREEZE_ALERT (critical — SF12 broadcast, all valves open)
  0x09 = WINDOW_OPEN_ALERT (zone conditioning pause)
  0x0A = ZONE_SETPOINT_UPDATE (hub → actuator, target temp + mode)
  0x0B = HEARTBEAT
  0x0C = ENERGY_REPORT (per-zone BTU/kWh)
  0x0D = SOLAR_STATUS (current production W, from hub to actuators)
  0x0E = TOU_SCHEDULE (current tariff rate, from hub)
  0x0F = COMFORT_VOTE (from tag: "I'm cold" / "I'm hot")
```

### BLE Comfort Tag Channel (nRF52840)

| Parameter | Value |
|-----------|-------|
| Profile | Custom GATT (comfort data + vote button) |
| Advertising | 2s (connected) / 5s (sleep) |
| Connection | Encrypted (LE Secure Connections) |
| Range | ~10m |
| Bonding | Phone + hub paired at setup |

---

## AI / ML Pipeline

### 1. Thermal Forecast Model (on hub, TFLite Micro)

- Input: 2-hour history of all room sensors (temp, MRT, humidity, occupancy) + outdoor temp + solar gain + weather forecast
- Model: Physics-informed RC-network + GRU correction (hybrid)
  - Physics layer: 5R-1C thermal network per room (resistance-capacitance), parameters learned per home
  - ML correction: GRU learns the residual between physics model and actual (captures effects physics can't: opening doors, body heat, cooking)
- Output: per-room temperature forecast for next 4 hours at 15-min resolution
- Size: ~180 KB (INT8 quantized)
- Runs every 15 min on hub
- Used by: comfort optimizer (decide whether to pre-heat now or wait), solar coordinator (use thermal mass as battery)

### 2. Personal Comfort Model (on cloud, per person)

- Input: skin temp, air temp, MRT, humidity, air velocity, HR, HRV, activity level, clothing estimate, time-of-day, season
- Output: thermal comfort vote prediction (PMV-style: -3 cold → +3 hot, target 0)
- Model: Gradient-boosted trees (XGBoost) with personal fine-tuning on vote data
- Training data: comfort tag votes ("I'm cold"/"I'm hot" button presses) + sensor context at time of vote
- Adapts: every 50 votes, model retrains with personal data; cold-tolerant vs heat-tolerant profiles emerge
- Deployed: compressed model pushed to hub (runs inference for each person per zone)
- Key insight: PMV (Predicted Mean Vote, ISO 7730) is population-average. ThermoGrid learns *your* personal deviation from PMV.

### 3. Routine / Occupancy Pattern Learning (on cloud)

- Input: 30-day occupancy history from all room sensors (mmWave + PIR per room)
- Model: HMM (Hidden Markov Model) with time-of-day states → room transitions
- Output: probability of each room being occupied for next 4 hours (per 15-min slot)
- Used by: comfort optimizer — pre-heat bedroom 30 min before predicted wake, pre-cool office before work-from-home start
- Adapts: weekly retraining; detects schedule changes (new job, vacation, guests)

### 4. Energy Optimization (on hub + cloud)

- Input: thermal forecast, occupancy prediction, TOU tariff schedule, solar production forecast, current zone states
- Model: MILP (Mixed Integer Linear Programming) optimization — minimize cost subject to comfort constraints
  - Decision variables: zone setpoints for next 4 hours (15-min resolution)
  - Constraints: comfort bounds per occupied zone, thermal capacity limits, valve/damper range, min temp (freeze protection)
  - Objective: minimize energy cost × tariff rate, maximize self-consumed solar
- Output: optimal zone setpoint schedule for next 4 hours → pushed to actuators every 15 min
- Cloud: heavy MILP solved on cloud (FastAPI backend), pushed to hub over MQTT; hub runs simplified heuristic if offline

### 5. Solar Self-Consumption Coordinator (on hub)

- Input: real-time solar production (from inverter API or CT clamp), current heating/cooling demand, thermal mass available
- Logic: when solar production > base load, ramp up heating/cooling zones (pre-heat/cool beyond setpoint using excess solar) — thermal mass stores it
- Output: temporary setpoint boosts to zone actuators ("solar boost: +2°C for 30 min")
- Result: 15-30% more of your solar used at home instead of exported at low feed-in rates

### 6. Open-Window & Anomaly Detection (on room sensor, edge)

- Edge rule: if temp drops >1°C in <60s AND air velocity >0.5 m/s → WINDOW_OPEN
- ML refinement (cloud): learns normal patterns; distinguishes open window from opened door from HVAC startup
- Action: hub pauses conditioning for that zone for 10 min (or until window closes), then resumes

---

## Cloud Dashboard

React.js web app + Python FastAPI backend.

- Real-time home thermal map (floor plan with per-room temp, MRT, occupancy color-coded)
- Energy dashboard: per-zone BTU/kWh, daily/weekly/monthly trends, cost × tariff
- Savings report: compares your usage to "what a single-thermostat home would have used" (counterfactual model)
- Solar self-consumption gauge: how much of your solar was used vs exported
- Per-person comfort profiles: view your learned thermal preferences, comfort vote history
- Zone schedule editor: set routines (wake, away, return, sleep) per zone
- Manual override: boost a zone ("make living room warmer for 1 hour")
- Weather integration: outdoor temp, solar forecast, wind (affects heat loss)
- Multi-home support (vacation home + primary)
- Alert log: freeze risk, window left open, valve fault, sensor offline, abnormal energy use

---

## Power Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    POWER DISTRIBUTION                                     │
│                                                                          │
│  HUB:    USB-C 5V ──► MCP73831 ──► LiPo 2500mAh                         │
│          (backup: hub + mesh survive 12+ hrs outage — freeze protect!)  │
│          AP2112-3.3 (logic) + AP6212-1.8 (flash)                        │
│                                                                          │
│  ROOM SENSOR: 2× AA (3V) + 0.5W solar ──► MCP73831 trickle              │
│           12+ months runtime; solar tops up during daylight              │
│           System-off <8µA, active ~20mA for <1s per measurement          │
│                                                                          │
│  ZONE ACTUATOR (wired): 24VAC from boiler/transformer                   │
│           → MP1584 buck → 5V → AP2112-3.3                               │
│           Motor/servo power from 24VAC directly                           │
│           (always-on, no battery needed for wired zones)                 │
│                                                                          │
│  ZONE ACTUATOR (wireless): 4× AA + boost converter to 12V for motor     │
│           6-12 months; motor peak ~300mA for <2s per adjustment          │
│                                                                          │
│  COMFORT TAG: CR2032 (3V)                                               │
│           8-12 months; system-off <15µA, ~5mA burst for 50ms             │
│           Optional: rechargeable LiPo + Qi coil                          │
└─────────────────────────────────────────────────────────────────────────┘
```

**Critical:** The hub has battery backup. If power is cut, the hub keeps the mesh alive, monitors for freeze risk, and can still command actuators (wired zones on 24VAC from a boiler that may still have power). Freeze protection is the one safety-critical path that must work without WiFi/cloud.

---

## Bill of Materials (Summary)

| Node | Est. BOM Cost | Key Components |
|------|--------------|----------------|
| Hub | ~$26 | RP2040, ESP32-C6, SX1262, ILI9488 TFT, W25Q256, PCF8563 RTC, LiPo 2500mAh |
| Room Sensor | ~$22 | STM32WL55JC, MLX90640, SHT45, SDP810, BMP390, HLK-LD2410B, AM612, ALS-PT19, AA + solar |
| Zone Actuator | ~$18 | ESP32-C3, SX1261, motorized valve (Danfoss RA2) or servo (MG996R), relays, DS18B20, flow sensor |
| Comfort Tag | ~$16 | nRF52840, MAX30208, TMP117, MAX30101, LSM6DSO, SHT40, CR2032 |
| **Total (4-node base system)** | **~$82** | Hub + 2 sensors + 1 actuator + 1 tag |
| **Full home (6 rooms)** | **~$190** | Hub + 6 sensors + 4 actuators + 2 tags |

See `hardware/bom/` for detailed per-node BOMs.

---

## Assembly Overview

1. **Hub:** Indoors, central location, USB-C power. Has display + battery backup. Wall-mount.
2. **Room sensors:** One per room, wall-mount at 1.5m height, away from direct sun/heat sources. AA + solar.
3. **Zone actuators:** Near each radiator / air handler / manifold. Wired (24VAC) or wireless (AA).
4. **Comfort tag:** Wear on wrist, chest, or clip to clothing. Pair via BLE in app.

Detailed assembly in `docs/assembly_guide.md`.

---

## Energy Savings Features

| Feature | Mechanism | Savings |
|---------|----------|---------|
| Zone conditioning | Only heat/cool occupied rooms | 15-25% |
| Predictive pre-heat | Use thermal mass during off-peak | 5-10% |
| Solar self-consumption | Ramp heating to match solar output | 10-20% of solar value |
| MRT-based comfort | Condition for actual comfort (not just air temp) | 5-8% (avoid over-conditioning) |
| Open-window detection | Pause zone when window opens | 3-5% |
| Personal comfort profiles | Don't overheat for cold-tolerant people | 3-7% |
| **Total** | | **20-40%** |

---

## Safety Features

| Feature | Detection | Response |
|---------|-----------|----------|
| Freeze protection | Any room <4°C | FREEZE_ALERT (SF12), all valves 100%, boiler ON |
| Pipe freeze (outdoor zone) | Outdoor/pipe temp <2°C | Activate trace heating relay, alert app |
| Window left open | WINDOW_OPEN + duration >30min | App alert "Window open in bedroom 35min" |
| Valve/actuator fault | Actuator not responding or stuck | Alert + reassign zone to backup actuator |
| Sensor offline | No heartbeat >10min | Mark zone "unknown", hold last setpoint, alert |
| Boiler/heat-pump failure | Pipe temp not rising despite valve open | Alert "Heating system may be down" |
| Overheat protection | Room temp >28°C despite cooling | Emergency: all cooling ON, alert |
| Power outage | Hub on battery | Maintain mesh + freeze protection; alert "Power out, on battery" |

---

## Getting Started

1. Set up the hub (USB-C power, connect to WiFi via mobile app over BLE)
2. Install room sensors (one per room, 5 min each — peel-and-stick mount)
3. Install zone actuators (replace TRV heads or install damper servos, 15-30 min per zone)
4. Wear comfort tag and pair via app
5. Walk through home calibration (`scripts/calibrate_thermal.py`) — opens/closes windows to learn airflow, measures each room's thermal mass
6. Set your comfort votes ("I'm cold" / "I'm hot" a few times to seed personal model)
7. Let it run for 3 days to learn your home's thermal model + routines
8. Watch your energy bill drop 🌡️⚡

---

## Project Structure

```
thermo-grid/
├── README.md              # This file
├── schematic/              # KiCad projects (one per node)
├── firmware/               # C source per node + shared common/
├── hardware/               # BOMs, enclosures
├── software/               # Cloud dashboard, ML pipeline, mobile app
├── scripts/                # Deployment, calibration
└── docs/                   # Architecture, API, protocol, assembly
```

---

## License

MIT — build it, sell it, improve it.

---

*Part of the [Devices](../README.md) collection — complex hardware+software systems that improve daily life.*