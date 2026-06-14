# BreathHome — Architecture Overview

## System Architecture

BreathHome is a 4-node indoor air quality monitoring and management system:

1. **Hub Node** — Central coordinator, WiFi/BLE bridge, local ML inference, TFT display
2. **Room Sensor Nodes** (1-16) — Multi-sensor air quality monitors per room
3. **HVAC Controller** — Zigbee + relay actuator for ventilation and filtration
4. **Wearable Breath Tags** (1-4) — Personal exposure monitors with BLE

All nodes communicate over a dedicated Sub-GHz (868/915MHz) LoRa mesh network for reliability. The hub bridges to WiFi for cloud analytics and the mobile app.

```
┌──────────────────────────────────────────────────────┐
│                    CLOUD LAYER                        │
│  ┌──────────┐  ┌──────────────┐  ┌────────────────┐ │
│  │ FastAPI   │  │ ML Pipeline  │  │ Mobile App     │ │
│  │ Dashboard │  │ Asthma risk  │  │ (React Native) │ │
│  │ REST+WS  │  │ Mold growth  │  │ Push alerts    │ │
│  │ PostgreSQL│  │ Filter life  │  │ Personal exp.  │ │
│  │ TimescaleDB│  │ Ventilation │  │ HVAC control   │ │
│  └─────┬─────┘  └──────┬───────┘  └───────┬────────┘ │
│        │               │                   │          │
│        └───────────────┼───────────────────┘          │
│                        │  MQTT + REST                  │
└────────────────────────┼────────────────────────────┘
                         │
┌────────────────────────┼────────────────────────────┐
│                   HUB NODE                            │
│  ┌─────────┐  ┌──────────┐  ┌───────────┐           │
│  │ nRF5340 │  │ ESP32-C6 │  │ SX1262    │           │
│  │ App+ML  │◄─►│ WiFi6   │  │ Sub-GHz   │           │
│  │ Mesh    │  │ Bridge  │  │ Mesh Coord │           │
│  │ BLE     │  │ MQTT    │  └─────┬──────┘           │
│  │ TFT/Spk │  │         │        │                   │
│  └─────────┘  └─────────┘        │                   │
│                                  │ Sub-GHz Mesh      │
└──────────────────────────────────┼───────────────────┘
                                   │
        ┌──────────────────────────┼──────────────────┐
        │                          │                   │
   ┌────┴─────┐             ┌──────┴──────┐    ┌──────┴──────┐
   │ ROOM     │             │ ROOM        │    │ HVAC        │
   │ SENSOR   │             │ SENSOR      │    │ CONTROLLER  │
   │ ×1-16    │             │ ×1-16       │    │ ×1          │
   │ STM32WB55│             │ STM32WB55   │    │ ESP32-S3    │
   │ SPS30    │             │ SCD41       │    │ CC2652R7    │
   │ SCD41    │             │ SGP41       │    │ SX1261      │
   │ SGP41    │             │ SFA30       │    │ 4× Relays   │
   │ SFA30    │             │ BME688      │    │ Zigbee      │
   │ BME688   │             │ TSL25911    │    │ SCT013     │
   │ TSL25911 │             │ SX1261      │    └──────┬──────┘
   │ RD200M*  │             └─────────────┘           │
   │ SX1261   │                                        │
   └──────────┘                                   Zigbee/433MHz
                                                         │
                                                  ┌──────┴──────┐
                                                  │ Smart Vents │
                                                  │ Purifiers   │
                                                  │ Range Hood  │
                                                  └─────────────┘

   ┌────────────────┐
   │ WEARABLE TAG   │  ◄─── BLE ───► Hub Node
   │ nRF52832       │
   │ SGP30 + SHT40  │
   │ LIS2DH12       │
   │ Vibr+LED+Btn   │
   └────────────────┘
```

## Data Flow

```
Sensor → Room Sensor Node → Sub-GHz Mesh → Hub Node → WiFi/MQTT → Cloud
                                                     ↓
                                              Local ML Inference
                                              (AQI, Mold Risk, Alerts)
                                                     ↓
                                              TFT Display + Voice
                                                     ↓
                                              BLE → Wearable Tag
                                              BLE → Mobile App
                                                     ↓
                                              HVAC Commands
                                              → HVAC Controller
                                              → Zigbee Smart Vents
                                              → 433MHz Range Hood
                                              → Relay Dry Contacts
```

## Communication Stack

| Layer | Protocol | Purpose |
|-------|----------|---------|
| L1 | Sub-GHz LoRa (868/915MHz) | Sensor-to-hub mesh, TDMA |
| L2 | BLE 5.0 | Wearable tags, mobile app |
| L3 | WiFi 6 (ESP32-C6) | Hub-to-cloud MQTT |
| L4 | Zigbee 3.0 (CC2652R7) | HVAC → smart devices |
| L5 | 433MHz OOK | HVAC → dumb appliances |
| L6 | MQTT/TLS | Cloud message broker |
| L7 | REST/WebSocket | Dashboard API |

## Power Budget

| Node | Average Current | Source | Battery Life |
|------|----------------|--------|--------------|
| Hub | 220mA | USB-C + 3000mAh Lipo | 14h battery backup |
| Room Sensor | 35mA | USB-C + 3×AA backup | 48h on AA |
| HVAC Controller | 120mA | 24VAC furnace transformer | Always on |
| Wearable Tag | 400µA | 120mAh Lipo | 36h on battery |

## Key Design Decisions

1. **Sub-GHz mesh for reliability**: LoRa at 868MHz penetrates walls far better than WiFi or BLE. Critical air quality alerts must not depend on WiFi being up.

2. **Local ML on every sensor**: AQI calculation and mold risk prediction run on each room sensor. This means alerts work even when the hub is unreachable.

3. **Closed-loop actuation**: The HVAC controller doesn't just sense — it acts. Smart vents, air purifiers, and exhaust fans are controlled automatically based on real-time air quality data.

4. **Personal exposure tracking**: The wearable tag tells you what YOU are breathing, not just what the room sensor reads. This is critical for asthma/COPD patients whose personal exposure differs from room averages.

5. **Radon monitoring**: The basement room sensor includes a radon detector (RadonEye RD200M). Radon is the #2 cause of lung cancer, yet most homes never test for it.

6. **Mold prediction**: By tracking humidity trends, VOC off-gassing patterns, and dew point, BreathHome predicts mold growth BEFORE it appears — not after.

7. **Filter health monitoring**: Instead of changing HVAC filters on a calendar, BreathHome uses duct pressure and blower current to tell you exactly when your filter needs replacing.