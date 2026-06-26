# PoolSync Architecture

## System Overview

PoolSync is a 4-node (5 with optional solar) pool monitoring, chemistry automation, and safety system built on Sub-GHz LoRa mesh, Wi-Fi 6, and cloud ML.

```
┌─────────────────────────────────────────────────────────────────────┐
│                         CLOUD LAYER                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐           │
│  │  FastAPI │  │   ML    │  │  Timescale│  │  MQTT    │           │
│  │ Dashboard│  │ Pipeline│  │    DB      │  │  Broker  │           │
│  └────┬─────┘  └────┬─────┘  └────┬──────┘  └────┬─────┘           │
│       │              │              │              │                │
│  ┌────▼──────────────▼──────────────▼──────────────▼─────┐         │
│  │              PoolSync Cloud API Gateway                │         │
│  │         (REST + WebSocket + MQTT + S3 Images)         │         │
│  └──────────────────────┬───────────────────────────────┘         │
└─────────────────────────┼──────────────────────────────────────────┘
                          │ HTTPS / MQTT / WebSocket
                          │
┌─────────────────────────┼──────────────────────────────────────────┐
│                   EDGE LAYER (Home)                                 │
│                         │                                           │
│  ┌──────────────────────▼──────────────────────────────────┐      │
│  │                  PoolSync Hub                             │      │
│  │   RP2040 (Sub-GHz radio, rules engine, display)         │      │
│  │   ESP32-S3 (Wi-Fi 6, BLE 5.0, cloud bridge, ML)        │      │
│  │                                                          │      │
│  │   ┌────────────────┐  ┌─────────────────┐               │      │
│  │   │  Local Rules    │  │  Edge ML Inf.   │               │      │
│  │   │  Engine         │  │  (AlgaeNet,     │               │      │
│  │   │  (pH dosing,   │  │   ClearWater,   │               │      │
│  │   │   freeze prot., │  │   SafetyNet)    │               │      │
│  │   │   safety lock)  │  │                 │               │      │
│  │   └────────────────┘  └─────────────────┘               │      │
│  └───────┬──────────┬──────────┬──────────┬────────────────┘      │
│          │          │          │          │                         │
│     Sub-GHz 868 MHz LoRa (TDMA)                                │
│          │          │          │          │                         │
│  ┌───────▼──┐ ┌────▼────┐ ┌───▼──────┐ ┌▼──────────┐            │
│  │ Chemistry│ │  Pool   │ │ Equipment│ │  Solar    │            │
│  │  Probe   │ │ Camera │ │ Controller│ │ Monitor  │            │
│  │  ×1-3   │ │         │ │          │ │ (optional)│            │
│  └──────────┘ └─────────┘ └──────────┘ └──────────┘            │
└──────────────────────────────────────────────────────────────────┘
```

## Hardware Nodes

### 1. PoolSync Hub (RP2040 + ESP32-S3)
- **Role**: Zone coordinator, local rules engine, cloud bridge
- **MCUs**: RP2040 (Cortex-M0+, 133 MHz) + ESP32-S3 (Xtensa LX7, 240 MHz)
- **Radio**: SX1262 868 MHz LoRa (2 km range) + Wi-Fi 6 + BLE 5.0
- **Display**: 2.8" IPS LCD (320×240)
- **Power**: 5V/3A USB-C (PoE optional)
- **Enclosure**: IP65 wall-mount, indoor/covered outdoor
- **Responsibilities**:
  - TDMA scheduling for Sub-GHz nodes
  - Local rules engine (pH dosing, freeze protection, safety interlocks)
  - Data aggregation and forwarding to cloud
  - Edge ML inference (AlgaeNet, ClearWater, SafetyNet)
  - OTA firmware updates for all nodes
  - 2.8" LCD status dashboard

### 2. Chemistry Probe ×1–3 (STM32L476RG)
- **Role**: Continuous water chemistry monitoring
- **MCU**: STM32L476RG (Cortex-M4, 80 MHz, ultra-low power)
- **Sensors**: ISFET pH, Platinum ORP, Amperometric free Cl, DS18B20 temp, Inductive conductivity, TSL2591 turbidity
- **Radio**: SX1262 868 MHz LoRa (TDMA)
- **Power**: 3× AA LiFeS2 (18-month battery life)
- **Enclosure**: IP68 titanium-body, submersible, chemical-resistant
- **Duty cycle**: Wake every 5 min, read sensors, transmit, sleep
- **Responsibilities**:
  - Sequential sensor excitation (pH → ORP → Cl → temp → conductivity → turbidity)
  - ISFET 2-point pH calibration
  - Battery management with 3-year projected life

### 3. Pool Camera (ESP32-S3)
- **Role**: Water clarity assessment, algae detection, safety monitoring
- **MCU**: ESP32-S3 (Xtensa LX7, 240 MHz, Wi-Fi 6, BLE 5.0)
- **Camera**: IMX477R 12MP 4K with IR-cut filter + 850nm IR array
- **Sensors**: TSL2591 ambient light, AM312 PIR motion
- **Audio**: MAX98357A 3W class-D speaker for verbal warnings
- **Storage**: 32GB eMMC edge image buffer
- **Power**: 5V/2A solar + LiPo 3.7V/4000mAh
- **Enclosure**: IP66 dome camera housing, pool-side pole mount
- **Responsibilities**:
  - On-device clarity scoring (histogram + green channel)
  - Day/night auto-switching (IR-cut + IR LED)
  - PIR-triggered safety alerts (unsupervised access)
  - Verbal warnings via speaker

### 4. Equipment Controller (STM32F407VG)
- **Role**: Pool equipment control and chemical dosing
- **MCU**: STM32F407VG (Cortex-M4, 168 MHz)
- **Outputs**: 8× SPDT 16A relays (pump, heater, lights, valves, blower)
- **Dosing**: 3× peristaltic pumps (acid, chlorine, clarifier) via A4988 stepper drivers
- **Sensors**: YF-S201 flow (dosing verification), MPX5010DP pressure (filter + entrapment), ACS712-30A current (GFCI)
- **Radio**: SX1262 868 MHz LoRa
- **Power**: 24VAC pool transformer → 5V/3A → 3.3V
- **Enclosure**: NEMA 4X fiberglass, lockable
- **Safety**: GFCI monitoring, entrapment pressure detection, VGB-compliant
- **Responsibilities**:
  - Real-time relay control and peristaltic pump dosing
  - Flow verification after dosing (YF-S201 confirms delivery)
  - Safety interlocks (GFCI → emergency shutdown, entrapment → pump off)
  - Freeze protection (auto pump on if water < 4°C)

### 5. Solar Monitor (optional, STM32L476RG)
- **Role**: Solar panel monitoring for solar-heated pools
- **MCU**: STM32L476RG
- **Sensors**: ML8511 UV irradiance, ACS712 solar pump current, DS18B20 panel + roof temp
- **Radio**: SX1262 868 MHz
- **Power**: Solar + LiPo (self-powered)
- **Responsibilities**: MPPT optimization data, solar gain estimation

## Communication Architecture

### Sub-GHz Layer (868 MHz LoRa)
- **Protocol**: PoolSync Protocol (PSP) binary frames
- **Modulation**: LoRa, SF9, BW 125 kHz, CR 4/5
- **Range**: 2 km line-of-sight
- **TDMA**: 10 slots per 1-second frame, hub is time master
- **Security**: AES-128-GCM encryption, per-node keys, replay protection
- **Slot allocation**:
  | Slot | Node | Direction |
  |------|------|-----------|
  | 0 | Hub TX | Hub → All |
  | 1 | Hub RX | All → Hub |
  | 2 | Probe 1 | Probe1 → Hub |
  | 3 | Probe 2 | Probe2 → Hub |
  | 4 | Probe 3 | Probe3 → Hub |
  | 5 | Camera | Camera → Hub |
  | 6 | Equipment | Equip → Hub |
  | 7 | Solar | Solar → Hub |
  | 8 | Alarm | Any (contention) |
  | 9 | Free | Unassigned |

### Wi-Fi Layer (ESP32-S3)
- **Protocol**: MQTT (QoS 1 for commands, QoS 0 for telemetry)
- **Security**: TLS 1.3, per-device certificates
- **Topics**: `poolsync/{pool_id}/telemetry`, `poolsync/{pool_id}/commands`, `poolsync/{pool_id}/images`

### Cloud Layer
- **API**: FastAPI (Python) — REST + WebSocket
- **Database**: TimescaleDB (time-series chemistry + equipment data)
- **Object Storage**: S3-compatible (images for cloud-based ML)
- **ML**: 6-model pipeline (see below)
- **Push**: Firebase Cloud Messaging for mobile notifications

## ML Pipeline

| # | Model | Input | Output | Architecture |
|---|-------|-------|--------|-------------|
| 1 | AlgaeNet | 72h chemistry + weather | 3-day algae probability | LSTM (128→64→32→3) |
| 2 | ChemBalance | Current chemistry + pool volume | Optimal dose (mL) | XGBoost |
| 3 | ClearWater | Pool image (4K) | Clarity score, algae class | MobileNetV3 → custom head |
| 4 | EnergyOpt | Chemistry + weather + TOU rates | Optimal schedule | DQN (reinforcement learning) |
| 5 | AnomalyDetect | Equipment telemetry stream | Fault probability | Autoencoder |
| 6 | SafetyNet | Camera frame | Person + distress detection | YOLOv8-nano + pose |

### Edge vs Cloud Split
- **Edge (ESP32-S3 Hub)**: ClearWater (TFLite INT8), SafetyNet (TFLite INT8), basic rules
- **Cloud**: AlgaeNet, ChemBalance, EnergyOpt, AnomalyDetect (full precision)

## Data Flow

```
Chemistry Probe → [Sub-GHz] → Hub → [Wi-Fi/MQTT] → Cloud
                               ↓
Camera → [Sub-GHz] → Hub → [Wi-Fi] → Cloud
                               ↓
Equipment ← [Sub-GHz] ← Hub ← [Cloud/MQTT] ← User Commands
                               ↓
Local Rules Engine (always operates, even without cloud)
  - pH dosing: automatic acid/chlorine dosing
  - Freeze protection: pump on when water < 4°C
  - Safety interlock: emergency shutdown on GFCI/entrapment
  - Dosing safety: flow verification, maximum dose limits
```

## Safety Architecture

PoolSync implements multiple safety layers:

1. **Hardware interlocks**: GFCI module, entrapment pressure sensor
2. **Firmware safety**: Hardcoded maximum dose limits, flow verification
3. **Local rules engine**: Operates without cloud, emergency pump shutdown
4. **Cloud monitoring**: Anomaly detection, push notifications for safety events
5. **Mobile alerts**: Immediate push notifications for GFCI fault, entrapment, unsupervised access
6. **Chemical limits**: Acid max 200mL/cycle, chlorine max 500mL/cycle, clarifier max 100mL/cycle
7. **VGB compliance**: Suction entrapment detection via pressure differential