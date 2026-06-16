# FreshKeep — Architecture Documentation

## System Overview

FreshKeep is a 4-node kitchen intelligence system that eliminates food waste, prevents kitchen fires, and manages your pantry — automatically.

### Design Philosophy

1. **Safety First**: Fire safety rules run locally on the Stove Guard MCU with sub-100ms response time. No cloud dependency for life-safety decisions.
2. **Privacy Preserving**: Stove Guard uses only a thermal camera (32×24 pixels) — no visual camera in the kitchen. Fridge/Pantry cameras are for food recognition only.
3. **Fail-Safe**: Gas valve is normally-closed (NC). Power loss = valve closes. Supercap backup ensures valve closure completes.
4. **Mesh Reliability**: Critical fire safety data uses Sub-GHz LoRa mesh — works without WiFi, penetrates walls and metal appliances.

## Block Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CLOUD (AWS/GCP)                              │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │
│  │ FastAPI    │  │ PostgreSQL  │  │ ML Pipeline               │  │
│  │ REST API   │  │ TimescaleDB │  │ Food Detection (EfficientDet)│  │
│  │ WebSocket  │  │             │  │ Spoilage Prediction (CNN+GRU)│  │
│  │ MQTT Bridge│  │             │  │ Fire Detection (Tiny MobileNet)│  │
│  └──────┬─────┘  └──────┬──────┘  │ Grocery Pattern (Transformer)│  │
│         │               │         └──────────────────────────┘  │
│         └───────┬───────┘                                       │
│                 │ MQTT                                           │
└─────────────────┼─────────────────────────────────────────────────┘
                  │
          ┌───────┴────────┐
          │  WiFi/BLE       │
          │  ESP32-C6       │
          │                 │
          │  ┌────────────┐ │
          │  │  RP2040    │ │      Sub-GHz LoRa Mesh (868MHz)
          │  │  Hub MCU   ├─┼──────────────────┬──────────────────┐
          │  │  Coordinator│ │                  │                  │
          │  │  + Display  │ │          ┌───────┴───────┐  ┌──────┴──────┐
          │  │  + Local    │ │          │  Fridge Node  │  │ Stove Guard │
          │  │    Rules    │ │          │  STM32L476    │  │ STM32F411   │
          │  └────────────┘ │          │  + Cameras    │  │ + Thermal   │
          │                 │          │  + Gas/VOC    │  │ + Gas/Smoke │
          │                 │          │  + Weights    │  │ + Valve     │
          │                 │          │  + SX1261     │  │ + Suppress  │
          └─────────────────┘          │  + Battery    │  │ + SX1261    │
                  │                    └───────┬───────┘  └──────┬──────┘
                  │                            │                  │
                  │ BLE                 ┌───────┴───────┐         │
                  │                     │  Pantry Node  │         │
                  └─────► Mobile App    │  ESP32-S3     │◄────────┘
                         (React Native) │  + Camera     │
                                        │  + Barcode    │
                                        │  + Weights    │
                                        │  + SX1261     │
                                        └───────────────┘
```

## Data Flow

### Sensor Data Flow (every 500ms)
1. **Stove Guard** (Slot 0): Thermal frame summary + gas readings → Hub
2. **Fridge Node** (Slot 1): Gas + temp + weights → Hub
3. **Pantry Node** (Slot 2): Weights + barcode + temp → Hub
4. **Hub** (Slot 3): Sync + commands broadcast
5. **ACK** (Slot 4): Retransmit if needed

### Hub Processing Pipeline
1. Receive sensor data from all nodes
2. Run local fire safety rules (sub-100ms response)
3. Run local spoilage estimation (TFLite Micro)
4. Aggregate data, buffer for WiFi upload
5. Generate shopping list locally (rule-based)
6. Upload to cloud via ESP32-C6 WiFi (every 5 seconds)
7. Broadcast alerts to mobile app via BLE

### Cloud Processing
1. MQTT broker receives sensor data
2. FastAPI persists to PostgreSQL/TimescaleDB
3. ML pipeline processes camera images for food identification
4. Spoilage prediction model refines estimates with historical data
5. Shopping pattern transformer generates purchase suggestions
6. Push notifications sent for alerts

## Communication Protocols

### Sub-GHz Mesh (Primary — Critical)
- **Frequency**: 868 MHz (EU) / 915 MHz (US)
- **Modulation**: LoRa SF7 (normal) / SF9 (alerts)
- **TDMA**: 500ms frame, 5 × 100ms slots
- **Priority**: Stove Guard always gets Slot 0
- **Range**: 30m indoor (through walls, around appliances)

### WiFi (Secondary — Cloud)
- **Protocol**: MQTT over TLS (QoS 1)
- **Frequency**: Every 5 seconds for sensor data
- **Images**: Uploaded on demand (WiFi only, not time-critical)

### BLE (Local — Mobile App)
- **Protocol**: GATT server on hub
- **Characteristics**: System status, inventory, alerts, commands
- **Range**: 10m (kitchen range)

## Power Management

### Hub Node
- **Source**: 5V USB-C (primary) + 2000mAh Lipo (backup)
- **Average**: 180mA (WiFi on) → ~11 hours on battery
- **Failsafe**: Auto-switches to battery on USB loss; mesh continues

### Fridge Node
- **Source**: 2200mAh cold-rated Lipo + Qi wireless charging
- **Average**: 3mA (one reading per minute + one TX per 500ms)
- **Battery Life**: ~25 days on a single charge
- **Charging**: USB-C magnetic connector when door is open

### Pantry Node
- **Source**: 5V USB-C (primary) + 1200mAh Lipo (backup)
- **Average**: 80mA (camera + barcode + WiFi provisioning)
- **Battery Backup**: ~10 hours

### Stove Guard
- **Source**: 24V DC hardwired (building electrical) + 5F supercap
- **Average**: 120mA @ 24V
- **Failsafe**: Supercap holds gas valve CLOSED for 30s after power loss
- **NC Valve**: Power loss = valve closes (failsafe)

## Safety Architecture

### Fire Detection (Sub-100ms Local Response)
1. **MLX90640 thermal array**: Detects pan overheating, oil flash points
2. **MQ-2 (LPG)**: Gas leak detection >300ppm
3. **MQ-135 (CO)**: Carbon monoxide >35ppm
4. **RE46C190 smoke detector**: Photoelectric smoke
5. **VS1838B (modified IR)**: Flame detection at 4.3µm wavelength
6. **Tiny MobileNet V1 ML model**: Distinguishes cooking from fire (thermal patterns)

### Automatic Responses
- **Gas shutoff**: Solenoid valve closes in <100ms on fire detection
- **Suppression**: Micro-pump sprays potassium bicarbonate for 3 seconds
- **Alarm**: 105dB siren + hub display + mobile push + SMS
- **Mesh broadcast**: FIRE_ALARM packet floods all nodes

### Unattended Cooking Detection
- If burner is on (thermal signature >120°C) and no motion for 10 minutes → WARNING
- If no motion for 20 minutes → auto gas shutoff + alarm
- Motion can be disabled for known long-cook items (slow cooker, etc.)

## Food Spoilage Detection

### Multi-Factor Spoilage Score (0-100)
| Factor | Weight | Sensor | Range |
|--------|--------|--------|-------|
| VOC Index | 30% | SGP40 | 0-500 |
| CO2 | 25% | SCD30 | 400-10000 ppm |
| Ethylene | 25% | MQ-3 | 0-4095 (ADC) |
| Temperature | 20% | SHT40 | -20°C to +5°C |

### Score Thresholds
| Score | Alert | Action |
|-------|-------|--------|
| 0-30 | ✅ Fresh | No action |
| 31-50 | ℹ️ Aging | Suggest recipes using item |
| 51-80 | ⚠️ Expiring | Push notification, priority cooking |
| 81-100 | 🔴 Spoiled | Discard alert, safety warning |

## Grocery Pattern Learning

### Input Features
- Purchase history (items, quantities, dates)
- Consumption rates (weight changes over time)
- Seasonal patterns (more fresh produce in summer, etc.)
- Expiry patterns (which items expire fastest)
- Recipe preferences (what you cook most)

### Output
- Predictive shopping list: "You'll need milk in 2 days"
- Reorder suggestions: "You buy eggs every 8 days, today is day 7"
- Recipe-aware: "You have chicken and peppers expiring — suggest stir-fry"
- Seasonal adjustments: "Berry season — strawberries are cheap and fresh"