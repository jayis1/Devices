# GrillSync — Protocol Specification

## 1. Physical Layer

### Sub-GHz (Grill Sentinel, Smoke Node)
- **Band:** 868 MHz (EU) / 915 MHz (US) ISM
- **Modulation:** LoRa (SX1262)
- **Spreading Factor:** SF7 (adaptive to SF11 for range)
- **Bandwidth:** 125 kHz
- **Coding Rate:** 4/5
- **Preamble:** 8 symbols
- **TX Power:** +22 dBm
- **Range:** 300 m LOS (SF7), 2 km (SF11 + mesh)
- **Encryption:** AES-128-CCM

### BLE 5.0 (Meat Probe)
- **Band:** 2.4 GHz
- **Range:** 15 m line-of-sight
- **GATT Service:** GrillSync Probe (custom UUID)
- **Telemetry Characteristic:** Notify, 18-byte payload
- **Config Characteristic:** Write, 4-byte payload
- **Connection Interval:** 250 ms (active cook)

## 2. Message Format

All Sub-GHz messages use a compact binary protocol:

```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x47 0x53│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
```

- **Sync:** `0x47 0x53` = "GS" (GrillSync)
- **Src:** Source node ID (0 = Hub, 1–15 = nodes)
- **Dst:** Destination (0xFF = broadcast)
- **MsgType:** Message type enum (see below)
- **MsgId:** 16-bit sequence number (per-node)
- **Payload:** 0–240 bytes (type-specific)
- **CRC16:** CRC-16-CCITT (polynomial 0x1021, init 0xFFFF)

## 3. Message Types

| Type | Name | Direction | Payload Size |
|------|------|-----------|-------------|
| 0x01 | JOIN_REQ | Node→Hub | 3 |
| 0x02 | JOIN_ACK | Hub→Node | 2 |
| 0x03 | TELEMETRY | Node→Hub | 18–24 |
| 0x04 | COMMAND | Hub→Node | 1+N |
| 0x05 | CMD_ACK | Node→Hub | 2 |
| 0x06 | ALERT | Node→Hub | 2+N |
| 0x07 | OTA_BLOCK | Hub→Node | 128 |
| 0x08 | OTA_ACK | Node→Hub | 3 |
| 0x09 | HEARTBEAT | Node→Hub | 2 |
| 0x0A | MESH_RELAY | Node→Node | N |
| 0x0B | DONENESS_UPDATE | Hub→All | 9 |
| 0x0C | TIME_SYNC | Hub→All | 4 |
| 0x0D | CONFIG | Hub→Node | N |
| 0x0E | CONFIG_ACK | Node→Hub | 2 |
| 0x0F | COOK_SESSION | Hub→All | 5 |
| 0x10 | THERMAL_FRAME | Sentinel→Hub | 6+N |
| 0x11 | SMOKE_QUALITY | Smoke→Hub | 4 |

## 4. Telemetry Payloads

### 4.1 Grill Sentinel (Sub-type 0x01, 24 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x01 |
| 1 | Battery | 1 | ×0.01 V (0xFF = USB) |
| 2 | Surface temp max | 2 | ×0.1°C (signed) |
| 4 | Surface temp avg | 2 | ×0.1°C (signed) |
| 6 | Hot zone count | 1 | count |
| 7 | Gas concentration | 2 | ×1 ppm |
| 9 | Gas LEL percent | 1 | ×1% LEL |
| 10 | Flame intensity | 1 | 0–255 |
| 11 | Flame detected | 1 | 0/1 |
| 12 | Ambient temp | 2 | ×0.1°C (signed) |
| 14 | Ambient humidity | 2 | ×0.1% RH |
| 16 | Acoustic energy | 2 | RMS ×100 |
| 18 | Flare-up risk | 1 | 0–100% |
| 19 | Flare-up ETA | 2 | ×100 ms |
| 21 | Event ID | 2 | counter |
| 23 | RSSI | 1 | signed dBm |

### 4.2 Meat Probe (Sub-type 0x02, 18 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x02 |
| 1 | Battery | 1 | ×0.01 V |
| 2 | Probe ID | 1 | 0–7 |
| 3 | Meat type | 1 | 0–7 enum |
| 4 | Temp tip | 2 | ×0.1°C (signed) |
| 6 | Temp mid | 2 | ×0.1°C (signed) |
| 8 | Temp surface | 2 | ×0.1°C (signed) |
| 10 | Temp ambient | 2 | ×0.1°C (signed) |
| 12 | Target temp | 2 | ×0.1°C (signed) |
| 14 | Doneness level | 1 | 0–5 enum |
| 15 | Doneness ETA | 2 | ×10 seconds |
| 17 | RSSI | 1 | signed dBm (0xFF for BLE) |

### 4.3 Smoke Node (Sub-type 0x03, 22 bytes)

| Offset | Field | Size | Unit |
|--------|-------|------|------|
| 0 | Sub-type | 1 | 0x03 |
| 1 | Battery | 1 | ×0.01 V (0xFF = USB) |
| 2 | PM1.0 | 2 | ×0.1 µg/m³ |
| 4 | PM2.5 | 2 | ×0.1 µg/m³ |
| 6 | PM10 | 2 | ×0.1 µg/m³ |
| 8 | VOC index | 2 | 0–500 |
| 10 | Gas resistance | 2 | ×100 Ω |
| 12 | CO₂eq | 2 | ×1 ppm |
| 14 | Smoke quality | 1 | 0–4 enum |
| 15 | Flame intensity | 1 | 0–255 |
| 16 | Temp | 2 | ×0.1°C (signed) |
| 18 | Humidity | 2 | ×0.1% RH |
| 20 | RSSI | 1 | signed dBm |

## 5. Alert Types

| Type | Alert Class | Severity | Trigger |
|------|-------------|----------|---------|
| 0x01 | GAS_LEAK | Critical | MQ-2 > 10% LEL |
| 0x02 | FLARE_UP_WARNING | High | FlareUpNet risk > 70%, ETA < 15s |
| 0x03 | FLARE_UP_ACTIVE | Critical | Flame detector + thermal spike |
| 0x04 | GRILL_FIRE | Critical | Thermal array max > 400°C + flame |
| 0x05 | FOOD_UNDERCOOKED | High | Meat below USDA temp after cook |
| 0x06 | FOOD_OVERCOOKED | Medium | Meat exceeds target by >5°C |
| 0x07 | PROBE_DISCONNECT | Medium | Thermocouple open-circuit |
| 0x08 | PROBE_LOW_BATTERY | Low | Battery < 3.3V |
| 0x09 | CHILD_IN_ZONE | High | Thermal human detection |
| 0x0A | PROBE_OVERTEMP | Critical | Probe body > 300°C |
| 0x0B | SMOKE_CREOSOTE | Medium | SmokeNet: creosote detected |
| 0x0C | NODE_OFFLINE | Low | Heartbeat missed > 60s |

## 6. Command Types

| Type | Command | Target |
|------|---------|--------|
| 0x01 | GAS_SHUTOFF | Hub relay |
| 0x02 | GAS_RESUME | Hub relay |
| 0x03 | BUZZER_ON | Hub buzzer |
| 0x04 | BUZZER_OFF | Hub buzzer |
| 0x05 | EMERGENCY_MODE | All nodes |
| 0x06 | NORMAL_MODE | All nodes |
| 0x07 | SET_CONFIG | Any node |
| 0x08 | REBOOT | Any node |
| 0x09 | CALIBRATE | Any node |
| 0x0A | START_COOK | Hub |
| 0x0B | STOP_COOK | Hub |
| 0x0C | SET_MEAT_PROFILE | Meat Probe |
| 0x0D | SET_TARGET_TEMP | Meat Probe |
| 0x0E | LED_RING_COLOR | Hub |
| 0x0F | SILENCE_ALERTS | Hub |

## 7. Thermal Frame Compression

The MLX90640 32×24 thermal array produces 768 pixels per frame. To fit
in mesh messages (240-byte payload max):

1. **Delta encoding:** Only pixels that changed >2°C since last frame
2. **8-bit quantization:** Temperature delta scaled to ±127 (0.5°C resolution)
3. **RLE compression:** Run-length encode zero-delta runs
4. **Result:** Typical frame = 40–120 bytes (15–50% of raw)
5. **Frame rate:** 2 Hz during active cook, 0.1 Hz idle

### Compression Format

```
┌───────────┬───────────┬───────────────────────────┐
│ Frame seq │ Max (2B)  │ Compressed data (N bytes) │
│ (1B)      │ Avg (2B)  │                            │
│           │ Zones (1B)│                            │
└───────────┴───────────┴───────────────────────────┘

Compressed data format:
- 0x00 + run_length: zero-delta run (N consecutive unchanged pixels)
- non-zero: int8 temperature delta (×0.5°C)
```

## 8. Meat Type & Doneness Profiles

### Meat Types

| Class | Meat Type | USDA Min °C |
|-------|-----------|-------------|
| 0 | Beef | 62.8°C (145°F) |
| 1 | Pork | 62.8°C (145°F) |
| 2 | Chicken/Poultry | 73.9°C (165°F) |
| 3 | Fish | 62.8°C (145°F) |
| 4 | Lamb | 62.8°C (145°F) |
| 5 | Veal | 62.8°C (145°F) |
| 6 | Game | 62.8°C (145°F) |
| 7 | Custom | User-set |

### Doneness Levels

| Level | Name | Beef | Pork | Lamb |
|-------|------|------|------|------|
| 0 | Raw | — | — | — |
| 1 | Rare | 52°C | — | 52°C |
| 2 | Medium Rare | 54°C | 60°C | 57°C |
| 3 | Medium | 60°C | 65°C | 63°C |
| 4 | Medium Well | 65°C | 70°C | — |
| 5 | Well Done | 71°C | 77°C | 71°C |

## 9. Join Process

1. Node powers on → sends `JOIN_REQ` (src=0xFF, dst=0x00)
2. Hub assigns node ID + TDMA slot → sends `JOIN_ACK`
3. Node stores assignment → begins operating in assigned slot
4. Hub broadcasts `TIME_SYNC` every 60 seconds

### JOIN_REQ Payload (3 bytes)

| Offset | Field | Size | Value |
|--------|-------|------|-------|
| 0 | Node type | 1 | 1=sentinel, 2=probe, 3=smoke |
| 1 | Battery | 1 | ×0.01 V (0xFF = USB) |
| 2 | FW version | 1 | e.g. 0x10 = v1.0 |

### JOIN_ACK Payload (2 bytes)

| Offset | Field | Size | Value |
|--------|-------|------|-------|
| 0 | Assigned node ID | 1 | 1–15 |
| 1 | TDMA slot | 1 | 1–15 |