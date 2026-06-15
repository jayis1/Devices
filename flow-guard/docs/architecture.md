# FlowGuard Architecture Document

## System Overview

FlowGuard is a multi-node water leak detection, pipe health monitoring, and flood prevention system designed for residential homes. It consists of 4 hardware node types communicating over Zigbee 3.0 mesh, with a cloud backend for ML inference and a mobile app for user interaction.

## Design Principles

1. **Fail-safe by default**: All valves spring-return to CLOSED on power loss
2. **Local-first**: Leak detection and valve shutoff work WITHOUT WiFi or cloud
3. **Privacy-first**: Sensor data stays local; cloud is for ML and remote access only
4. **Multi-layer detection**: Conductive traces (instant) → acoustic ML (<3s) → pressure anomaly (<5s) → NILM flow analysis (<30s)
5. **Mesh resilience**: Zigbee mesh self-heals; pipe sensors double as routers
6. **Ultra-low power**: Pipe sensors run 7+ years on a coin cell

## Data Flow

```
Sensor Nodes                    Hub                           Cloud
─────────────                  ─────────                     ─────
Pipe Sensor ──┐                                              
               │  Zigbee 3.0   ┌─────────┐    WiFi/MQTT    ┌────────────┐
Pipe Sensor ──┤──────────────►│  nRF52840 │──────────────►│ FastAPI    │
               │   mesh       │  +ESP32-C6│               │ Backend    │
Appliance ────┤               │           │               │            │
Monitor       │               │ TFLite   │               │ NILM ML    │
               │               │ Local ML │               │ Freeze ML  │
Valve         │◄─────────────│           │◄──────────────│ Push Notif  │
Controller ───┘  Commands    └─────────┘    Cloud cmds   └────────────┘
                                                      │
                                              ┌───────┴──────┐
                                              │  React Native│
                                              │  Mobile App  │
                                              └──────────────┘
```

## Communication Architecture

### Zigbee 3.0 Mesh Network
- **Coordinator**: Hub node (nRF52840)
- **Routers**: Valve controller, pipe sensors (relay traffic)
- **End devices**: Appliance monitors (sleep between transmissions)
- **Security**: AES-128-CCM with network key + per-device link key
- **Commissioning**: Touchlink (BLE from mobile app) or Install Code

### MQTT Cloud Bridge
- Hub bridges Zigbee data to MQTT broker (Mosquitto)
- Topics:
  - `flowguard/sensors/pipe/{node_id}` — Pipe sensor reports
  - `flowguard/sensors/appliance/{node_id}` — Appliance monitor reports
  - `flowguard/valve/status` — Valve controller status
  - `flowguard/alerts/{level}` — Alert notifications
  - `flowguard/commands/valve` — Valve commands (from app/cloud)

### BLE (Hub ↔ Mobile App)
- Used for initial pairing and local configuration
- GATT server on hub with characteristics for:
  - System status (read)
  - Valve control (write, requires authentication)
  - WiFi configuration (write)
  - Firmware update (write)

## Safety Architecture

### Valve Shutoff Decision Hierarchy

```
Level 1: LOCAL INSTANT (<0.5s)
  - Conductive trace wet → IMMEDIATE valve close
  - No hub processing needed, valve controller auto-closes

Level 2: HUB LOCAL ML (<3s)
  - Acoustic leak classification >0.85 confidence → valve close
  - Works without WiFi or cloud

Level 3: HUB FLOW ANALYSIS (<5s)
  - Pressure drop >20 PSI in <5s → valve close
  - Unknown high flow (>30 L/min) → valve close

Level 4: CLOUD ML (<30s)
  - NILM flow disaggregation identifies unknown usage
  - Freeze risk prediction enables heat trace
  - Appliance anomaly detection (running toilet, etc.)
```

### Failsafe Mechanisms

| Failure Mode | System Behavior |
|-------------|-----------------|
| WiFi outage | All local detection and shutoff still works; data buffered on SD card |
| Cloud outage | All local detection works; cloud features (NILM, freeze prediction) unavailable |
| Hub power loss | Valve spring-returns to CLOSED; sensor nodes continue mesh operation |
| Hub firmware crash | Watchdog timer resets hub; valve spring-returns during reset |
| Valve controller power loss | Valve spring-returns to CLOSED; 18650 battery reports power loss |
| Valve motor failure | Manual override handle; spring-return ensures CLOSED position |
| Zigbee mesh failure | Each node operates independently; valve controller auto-closes on flow anomaly |
| Battery low | Push notification at 20%; critical valve operations at 5% |
| Sensor node battery dead | Last known state preserved; hub alerts "sensor offline" |

## Power Budget

| Node | Average Current | Battery Life |
|------|----------------|--------------|
| Hub | 120mA (WiFi on) | 22 hours (18650) |
| Valve Controller | 5mA idle, 300mA active | 7+ days (18650 backup) |
| Pipe Sensor | 15µA average | 7.5 years (CR2477) |
| Appliance Monitor | 20µA average | 4 years (2× AA) |

## Security Model

1. **Zigbee Network**: AES-128-CCM encryption with per-device link keys
2. **WiFi**: WPA3, TLS 1.3 for MQTT broker connection
3. **MQTT**: Username/password authentication, TLS encrypted
4. **API**: JWT authentication, HTTPS only
5. **Valve Control**: 2FA required for OPEN commands (SMS or TOTP)
6. **BLE**: Pairing requires physical button press on hub
7. **OTA**: Signed firmware images, SHA-256 verification

## Regulatory Considerations

- **FCC Part 15**: Zigbee 2.4GHz (certified module)
- **UL 2075**: Gas and vapor detectors and sensors (relevant for leak detection)
- **IPC/IP Rating**: Valve controller IP54, pipe sensors IP54, hub IP30
- **Plumbing Code**: Valve installation must comply with local plumbing code (typically requires licensed plumber for main water line work)