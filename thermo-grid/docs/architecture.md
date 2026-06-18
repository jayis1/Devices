# ThermoGrid — Architecture & Protocol Docs

## System Overview

ThermoGrid is a multi-node IoT system for intelligent home thermal comfort and energy optimization:

| Node | MCU | Role | Power |
|------|-----|------|-------|
| Hub | RP2040 + ESP32-C6 | Mesh coordinator, thermal forecast ML, comfort optimizer, solar/TOU coordinator, cloud bridge | USB-C + LiPo backup |
| Room Sensor | STM32WL55JC | Air temp, MRT (thermal IR), humidity, air velocity, occupancy, CO2, solar gain | 2× AA + solar |
| Zone Actuator | ESP32-C3 + SX1261 | Radiator valve / damper / relay control, PID loop, energy accounting | 24VAC or 4× AA |
| Comfort Tag | nRF52840 | Skin temp, HR, activity, personal comfort prediction, vote button | CR2032 (8-12 months) |

## Mesh Protocol Specification

### Physical Layer
- **Radio:** SX1261/62 / STM32WL55 integrated, 915MHz (US) / 868MHz (EU)
- **Modulation:** LoRa — SF7 (normal), SF9 (actuator commands), SF12 (freeze alert)
- **Bandwidth:** 125kHz
- **TX Power:** +14dBm (nodes), +20dBm (hub)
- **Range:** 30m indoor (typical), 100m (SF9), 300m+ (SF12)
- **Sync Word:** 0x5447 ("TG")

### MAC Layer
- **Access:** TDMA (Time Division Multiple Access) — dynamic frame
- **Hub is coordinator:** broadcasts sync beacon in Slot 0
- **Frame:** (N+2) slots × 100ms where N = number of sensors/actuators
- **Slot 0:** Hub broadcast (sync + zone setpoints + solar + TOU)
- **Slots 1..N:** Each sensor/actuator uplinks (temp, occupancy, valve pos, etc.)
- **Slot N+1:** Control / ACK / retransmit / OTA

### Freeze Protection Override
- When any room sensor reports <4°C, it immediately broadcasts
  a FREEZE_ALERT packet on SF12 (max range + robustness)
- Hub halts normal TDMA, forces ALL valves to 100% open
- Boiler/heat-pump relay forced ON
- Alert pushed to cloud/app (if WiFi available)
- Normal TDMA resumes after all rooms >6°C for 5 minutes

### Window-Open Override
- Room sensor detects window open (temp drop + air velocity + humidity change)
- Hub sends VALVE_CLOSE to that zone's actuator
- Zone conditioning paused for 10 min (or until window closes)
- Alert: "Window open in bedroom — heating paused"

### Network Layer
- **Addressing:** 8-bit node IDs
  - 0x00 = Hub
  - 0x10-0x7F = Sensors and actuators (assigned at enrollment)
  - 0x80-0x8F = Comfort tags (paired persons)
  - 0xFF = Broadcast
- **Timeout:** Node marked inactive after 300s without heartbeat

### Application Layer
- Packet types: SENSOR_DATA, ACTUATOR_DATA, COMFORT_DATA, COMMAND, ACK,
  OTA_BLOCK, CALIBRATION, FREEZE_ALERT, WINDOW_OPEN, ZONE_SETPOINT,
  HEARTBEAT, ENERGY_REPORT, SOLAR_STATUS, TOU_SCHEDULE, COMFORT_VOTE
- See `firmware/common/mesh_protocol.h` for full struct definitions

## BLE Comfort Tag Channel (nRF52840)

| Parameter | Value |
|-----------|-------|
| Profile | Custom GATT (comfort data + vote button) |
| Advertising | 2s (connected) / 5s (sleep) |
| Connection | Encrypted (LE Secure Connections) |
| Range | ~10m |
| Bonding | Phone + hub paired at setup |
| Manufacturer data | person_id, comfort_score, battery_pct (in advertisement) |

## WiFi / BLE Bridge (ESP32-C6 on Hub)

### MQTT Topics
- `thermogrid/sensor_data` — Room sensor telemetry (JSON)
- `thermogrid/actuator_data` — Actuator telemetry (JSON)
- `thermogrid/comfort_data` — Comfort tag data (JSON)
- `thermogrid/zone_state` — Zone state updates (JSON)
- `thermogrid/energy_report` — Per-zone energy (JSON)
- `thermogrid/freeze_alert` — Critical freeze alert (JSON)
- `thermogrid/window_open` — Window open event (JSON)
- `thermogrid/comfort_vote` — Comfort vote from tag (JSON)
- `thermogrid/solar_status` — Solar production status (JSON)
- `thermogrid/tou_schedule` — Current tariff schedule (JSON)
- `thermogrid/alerts` — General system alerts (JSON)
- `thermogrid/commands/setpoint` — Set zone setpoint
- `thermogrid/commands/boost` — Temporary zone boost
- `thermogrid/commands/schedule` — Zone schedule update
- `thermogrid/forecast/update` — Push updated thermal forecast to hub
- `thermogrid/optimize/result` — Push optimized setpoint schedule to hub

## ML Pipeline

### On-Device (TFLite Micro)
1. **Thermal forecast** (hub, ~180 KB) — Physics RC-network + GRU correction, 4h ahead
2. **Comfort prediction** (hub, ~60 KB) — Personal comfort score from tag data

### Cloud
3. **Personal comfort model** — XGBoost per person, trained on vote data
4. **Routine / occupancy** — HMM learns when each room is typically occupied
5. **Energy optimization** — MILP: minimize cost × tariff subject to comfort constraints
6. **Solar coordinator** — Real-time solar production → zone boost decisions

## Power Architecture

| Node | Source | Backup | Lifetime |
|------|--------|--------|----------|
| Hub | USB-C 5V | LiPo 2500mAh | 12+ hrs on battery |
| Room Sensor | 2× AA + 0.5W solar | — | 12+ months (solar trickle) |
| Zone Actuator (wired) | 24VAC | — | continuous (always-on) |
| Zone Actuator (wireless) | 4× AA | — | 6-12 months |
| Comfort Tag | CR2032 | — | 8-12 months |

## Security Considerations

- Freeze protection path (sensor → Sub-GHz → hub → actuator) works without WiFi
- Hub battery backup keeps mesh + freeze protection alive during power outage
- BLE comfort tag uses LE Secure Connections (AES-CCM) encryption
- Zone setpoints are time-stamped and authenticated (CRC + sync word)
- Boiler relay is opto-isolated from logic (safety)
- Valve stuck detection: actuator reports if valve position doesn't match commanded
- Overtemp protection: if room >28°C despite cooling, emergency alert