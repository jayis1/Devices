# SightSync — Architecture Document

## 1. System Overview

SightSync is a 4-node hardware + software system for eye health and digital eye-strain prevention.

```
                     ┌─────────────────┐
                     │   SightSync      │
                     │     Cloud        │
                     │ FastAPI + MQTT   │
                     │ + TimescaleDB    │
                     └───────▲─────────┘
                             │ Wi-Fi
                     ┌───────┴─────────┐
                     │   Vision Hub     │
                     │   ESP32-S3       │
                     │ BLE + Sub-GHz   │
                     └───▲───────▲─────┘
            BLE 5.0       │       │   Sub-GHz 868 MHz
                     ┌────┴──┐ ┌──┴──────────┐
                     │Eye Tag│ │Desk Sentinel │
                     │nRF52840│ │ESP32-S3     │
                     └───────┘ └─────────────┘
                                 ┌─────────────┐
                                 │Lamp Node    │
                                 │RP2040       │
                                 └─────────────┘
```

## 2. Node Responsibilities

### Vision Hub (ESP32-S3)
- BLE central: connects to Eye Tag, receives blink/posture/temp data
- Sub-GHz coordinator: TDMA hub, receives desk/lamp data, sends lamp commands
- Edge ML: Visual Fatigue Index (XGBoost), blink anomaly (isolation forest)
- 20-20-20 timer engine
- E-ink display, speaker, haptic, LED ring for alerts
- Wi-Fi + MQTT to cloud

### Desk Sentinel (ESP32-S3)
- VL53L1X ToF: measures eye-to-screen distance at 1 Hz
- VEML7700: ambient illuminance
- TCS34725: RGBC color → CCT estimation
- APDS9306: blue-light irradiance
- SSD1306 OLED: local distance display
- Near-work dose accumulation
- Sub-GHz to hub

### Wearable Eye Tag (nRF52840)
- IR LED + photodiode: blink detection at 50 Hz (940 nm reflectance)
- TMP117: periocular skin temperature (±0.1°C)
- LSM6DSO: head posture (complementary filter, forward-head detection)
- APDS9306: personal blue-light dose
- BLE 5.0 to hub
- 2× CR2032, ~18-day battery with adaptive sampling

### Smart Lamp Node (RP2040)
- TLC5971: drives 60× warm-white + 60× cool-white LEDs
- VEML7700: closed-loop brightness regulation
- CC1101: receives lamp commands (CCT + brightness) from hub
- Rotary encoder + button: manual override
- 12V/2A powered

## 3. Data Flow

```
Eye Tag ──BLE──► Hub ┌── edge ML (fatigue index)
                        ├── 20-20-20 timer
                        ├── blink anomaly
                        ├── e-ink display
                        ├── alerts (haptic/audio/LED)
                        └── Wi-Fi/MQTT ──► Cloud ──► Mobile App

Desk Sentinel ──Sub-GHz──► Hub ┌── distance alarm
                                  ├── ambient light logging
                                  └── blue-light dose

Hub ──Sub-GHz──► Lamp Node ┌── CCT + brightness commands
                              └── adaptive lighting

Hub ──Wi-Fi──► Cloud ┌── myopia LSTM (90-day forecast)
                       ├── circadian DQN policy
                       ├── daily visual hygiene score
                       └── optometrist report
```

## 4. Communication Topology

- **BLE 5.0**: Eye Tag → Hub (star, peripheral→central). 2.4 GHz, 5m range.
- **Sub-GHz 868 MHz**: Hub ↔ Desk Sentinel, Hub ↔ Lamp Node (TDMA mesh). 50m indoor.
  - Hub heartbeat at T+0
  - Desk Sentinel TX slot at T+100ms
  - Lamp Node TX slot at T+200ms
  - ACK window T+300-400ms
- **Wi-Fi 4**: Hub → Cloud (MQTT over TLS). 2.4 GHz.

## 5. Edge vs Cloud ML

| Model | Location | Reason |
|-------|----------|--------|
| Fatigue Index | Hub (edge) | Real-time, low-latency, privacy |
| Blink Anomaly | Hub (edge) | Real-time, privacy |
| Posture Risk | Hub (edge) | Real-time, low-latency |
| Myopia Forecast | Cloud | Requires longitudinal data, heavy compute |
| Circadian DQN | Cloud trains, lamp executes | Policy optimization needs data |
| Dry-Eye Risk | Cloud | Requires multi-day patterns |