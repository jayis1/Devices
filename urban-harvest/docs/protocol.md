# UrbanHarvest — Mesh Protocol Specification

## Physical Layer

| Parameter | Value |
|-----------|-------|
| Frequency | 868.0 MHz (EU) / 915.0 MHz (US) |
| Modulation | LoRa (Semtech SX1262/SX1261/STM32WL) |
| Spreading Factor | SF7 (normal), SF10 (weather station long range) |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| TX Power | +14 dBm (EU), +20 dBm (US) |
| Preamble | 4 symbols |
| Sync Word | 0xUH01 (UrbanHarvest mesh) |

## MAC Layer: TDMA

The hub is the coordinator and assigns time slots to all nodes.

### Frame Structure

```
Slot 0:    Hub sync broadcast (commands, slot assignments, time sync)
Slots 1-12:  Plant sensors 1-12 uplink
Slot 13:    Grow pod uplink status
Slot 14:    Weather station uplink
Slots 15-24: Plant sensors 13-24 (if present)
Slot 25:    Alert / retransmit / control

Each slot: 100ms
Total frame: 2600ms (26 slots)
```

### Slot Assignment

- Hub always uses slot 0
- Plant sensors assigned slots 1-24 in order of registration
- Grow pod assigned slot 13
- Weather station assigned slot 14
- Slot 25 is shared (CSMA for urgent alerts, scheduled ACK for normal)

### Join Procedure

1. New node listens for hub sync broadcast (slot 0)
2. Node sends HEARTBEAT on slot 25 with its type and capabilities
3. Hub responds with slot assignment in next sync broadcast
4. Node begins transmitting on assigned slot

## Packet Format

```
[ PREAMBLE(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | SEQ(2) | PAYLOAD(0-48) | CRC16(2) ]

Total: up to 56 bytes

PREAMBLE:  0xAA, 0x55
LEN:       Total packet length (excluding preamble)
SRC_ID:    Source node (0=hub, 1-24=plant sensors, 0x40=grow pod, 0x80=weather)
DST_ID:    Destination (0=hub, 0xFF=broadcast)
TYPE:      Message type (see below)
SEQ:       Sequence number (incremented per message, wraps at 65535)
PAYLOAD:   Up to 48 bytes, format depends on TYPE
CRC16:     CRC16-CCITT over SRC_ID through end of PAYLOAD
```

## Message Types

| Type | ID | Direction | Payload Format | Description |
|------|-----|-----------|----------------|-------------|
| SOIL_DATA | 0x01 | Sensor→Hub | 12 bytes | Soil moisture, EC, temp, PAR, health, leaf wetness |
| LIGHT_DATA | 0x02 | Sensor→Hub | 6 bytes | PAR + lux reading |
| LEAF_WETNESS | 0x03 | Sensor→Hub | 4 bytes | Leaf wetness % + duration |
| GROW_POD_STATUS | 0x04 | Pod→Hub | 17 bytes | Pump, nutrients, climate, lights, disease |
| WEATHER_DATA | 0x05 | Weather→Hub | 20 bytes | Temp, RH, pressure, wind, rain, UV, battery |
| IRRIGATION_CMD | 0x06 | Hub→Pod | 5 bytes | Plant ID, volume, duration |
| NUTRIENT_CMD | 0x07 | Hub→Pod | 10 bytes | Plant ID, A/B/pH volumes |
| LIGHT_CMD | 0x08 | Hub→Pod | 5 bytes | R/B/W/FR PWM values |
| DISEASE_ALERT | 0x09 | Pod→Hub | 5 bytes | Plant ID, class, confidence, image ID |
| ACK | 0x0A | Any→Any | 2 bytes | Acknowledged SEQ number |
| OTA_BLOCK | 0x0B | Hub→Any | 48 bytes | Firmware update chunk |
| HARVEST_PREDICT | 0x0C | Hub→Any | 6 bytes | Plant ID, days, yield estimate |
| HEARTBEAT | 0x0D | Any→Hub | 4 bytes | Node type, battery, status |
| CALIBRATION | 0x0E | Hub→Sensor | 12 bytes | Calibration constants |
| DANGER_ALERT | 0x0F | Any→Hub | 6 bytes | Alert type, node, value (bypasses TDMA!) |
| CAMERA_READY | 0x10 | Pod→Hub | 4 bytes | Image ID, size (retrieve via WiFi) |

## SOIL_DATA Payload (12 bytes)

| Offset | Size | Field | Scale | Range |
|--------|------|-------|-------|-------|
| 0 | 1 | moisture_pct | Direct | 0-100% |
| 1-2 | 2 | ec_x10 | EC × 10 | 0.0-5.0 mS/cm |
| 3 | 1 | temp_c_offset | °C + 40 | -40 to +85°C |
| 4-5 | 2 | par_x10 | PAR × 10 | 0-6553 µmol/m²/s |
| 6 | 1 | health_index | Direct | 0-100 |
| 7 | 1 | leaf_wet_pct | Direct | 0-100% |
| 8 | 1 | battery_x20 | V × 20 | 0-6.0V |
| 9 | 1 | health_category | Direct | 0-4 |
| 10-11 | 2 | leaf_wet_h_x10 | Hours × 10 | 0-6553 h |

## WEATHER_DATA Payload (20 bytes)

| Offset | Size | Field | Scale | Range |
|--------|------|-------|-------|-------|
| 0-1 | 2 | temp_x10 | °C × 10 | -40.0 to +85.0°C |
| 2-3 | 2 | rh_x10 | % × 10 | 0-100% |
| 4-5 | 2 | pressure_x10 | hPa × 10 | 900-1100 hPa |
| 6-7 | 2 | wind_x10 | km/h × 10 | 0-200 km/h |
| 8 | 1 | wind_dir | 0-7 | N,NE,E,SE,S,SW,W,NW |
| 9-10 | 2 | rain_x100 | mm × 100 | 0-655 mm |
| 11-12 | 2 | uv_x10 | UV × 10 | 0-15 |
| 13-14 | 2 | light_lux | Direct | 0-65535 lux |
| 15 | 1 | solar_v_x20 | V × 20 | 0-6.0V |
| 16 | 1 | bat_v_x20 | V × 20 | 0-6.0V |
| 17 | 1 | bat_soc | Direct | 0-100% |
| 18 | 1 | pressure_trend | 1=rising, 2=steady, 3=falling | - |
| 19 | 1 | rain_predicted | 0/1 | - |

## Alert Priority

| Priority | Behavior |
|----------|----------|
| Normal | Sent in assigned TDMA slot |
| Urgent | Sent in slot 25 (shared alert slot) |
| Critical | Sent IMMEDIATELY on slot 25 with CSMA, bypasses TDMA. Retries 3x. Example: pump failure, soil critically dry, wind danger |

## CRC16-CCITT

- Polynomial: 0x1021
- Initial value: 0xFFFF
- Computed over SRC_ID through end of PAYLOAD (excludes preamble and CRC itself)
- Verification: Receiver recomputes CRC and compares