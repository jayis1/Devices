# HearthKeep Protocol Specification

## Overview

HearthKeep uses a custom Sub-GHz LoRa mesh protocol for communication between the hub and all nodes (room monitors, bed mat). Wearable tags communicate via BLE 5.0. The hub bridges to cloud services via WiFi/MQTT.

## Sub-GHz Mesh Protocol

### Physical Layer

| Parameter | EU (868MHz) | US (915MHz) |
|-----------|-------------|-------------|
| Frequency | 868.0 MHz | 915.0 MHz |
| Modulation | LoRa | LoRa |
| Spreading Factor | SF7 (normal) / SF9 (long range) | SF7 (normal) / SF9 (long range) |
| Bandwidth | 125 kHz | 125 kHz |
| TX Power | +14 dBm | +20 dBm |
| Range (indoor) | 30m (SF7) / 100m (SF9) | 40m (SF7) / 130m (SF9) |
| Data Rate | ~5.5 kbps (SF7) | ~5.5 kbps (SF7) |
| Preamble | 8 symbols | 8 symbols |
| CRC | CRC-16/CCITT | CRC-16/CCITT |
| Sync Word | 0x484B ("HK") | 0x484B ("HK") |

### TDMA Frame Structure

```
Frame (900ms):
┌────────┬────────┬────────┬─────┬────────┬────────┬────────┬─────┬────────┐
│ SLOT 0 │ SLOT 1 │ SLOT 2 │ ... │ SLOT 8 │ SLOT 9 │ SLOT10 │ ... │ SLOT17 │
│  HUB   │  RM 1  │  RM 2  │     │  RM 8  │BED MAT │  RM 9  │     │ CTRL  │
│ 50ms   │ 50ms   │ 50ms   │     │ 50ms   │ 50ms   │ 50ms   │     │ 50ms   │
└────────┴────────┴────────┴─────┴────────┴────────┴────────┴─────┴────────┘

Slot 0: Hub broadcasts sync, commands, and acknowledgments
Slots 1-8: Room monitors 1-8 uplink data
Slot 9: Bed mat uplink data
Slots 10-15: Room monitors 9-14 (if present)
Slot 16: Reserved / expansion
Slot 17: Control, ACK, retransmit, and ALERT OVERRIDE

ALERT OVERRIDE: Fall detection (FALL_ALERT type) can transmit
on Slot 17 regardless of TDMA schedule using CSMA fallback.
```

### Packet Format

```
Byte:  0-3      4-5     6      7      8      9      10-11    12-(N-2)  (N-1)-N
Field: PREAMBLE SYNC    LEN    SRC_ID DST_ID TYPE   SEQ_NUM  PAYLOAD   CRC16
Size:  4 bytes  2 bytes 1 byte 1 byte 1 byte 1 byte 2 bytes  0-48     2 bytes

Total packet: 10 + payload_len + 2 bytes (min 12, max 60 bytes)
```

**Preamble**: `0x48 0x4B 0x48 0x4B` ("HKHK" — allows receiver to detect start of packet)

**Sync Word**: `0x484B` ("HK" — confirms this is a HearthKeep packet)

**Length**: Total packet length including header and CRC (12-60)

**SRC_ID**: Source node ID
- `0x00`: Hub
- `0x01-0x08`: Room monitors 1-8 (TDMA slots 1-8)
- `0x09-0x0E`: Room monitors 9-14 (TDMA slots 10-15)
- `0x0A`: Bed mat (TDMA slot 9)
- `0xF0-0xF3`: Wearable tags 1-4 (BLE only)
- `0xFF`: Broadcast

**DST_ID**: Destination node ID
- `0x00`: Hub (uplink)
- `0xFF`: Broadcast
- `0x01-0x0E`: Specific room monitor
- `0x0A`: Bed mat

**TYPE**: Packet type (see below)

**SEQ_NUM**: Sequence number (wraps around at 65535). Used for deduplication and ordering.

**PAYLOAD**: Type-specific data (0-48 bytes)

**CRC16**: CRC-16/CCITT over all bytes except preamble. Polynomial: 0x1021, init: 0x1D0F.

### Packet Types

| Type | ID | Direction | Payload Size | Description |
|------|-----|-----------|-------------|-------------|
| RADAR_DATA | 0x01 | RM → Hub | 20 bytes | Radar presence/fall data |
| ENV_DATA | 0x02 | RM → Hub | 17 bytes | Environment sensor data |
| BED_VITALS | 0x03 | BM → Hub | 26 bytes | Heart rate, breathing, sleep |
| COMMAND | 0x04 | Hub → Node | 2-18 bytes | Configuration command |
| ACK | 0x05 | Both | 2 bytes | Acknowledgment |
| OTA_BLOCK | 0x06 | Hub → Node | 48 bytes | Firmware update chunk |
| FALL_ALERT | 0x07 | RM → Hub | 14 bytes | CRITICAL fall detection |
| PANIC_ALERT | 0x08 | Tag → Hub | 8 bytes | Panic button press |
| HEARTBEAT | 0x09 | All → Hub | 7 bytes | Periodic alive signal |
| CALIBRATION | 0x0A | Both | 33 bytes | Sensor calibration data |
| LOW_BATTERY | 0x0B | Node → Hub | 4 bytes | Battery warning |

### Payload Formats

#### RADAR_DATA (0x01) — 20 bytes

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 1 | presence_count | uint8 | Number of people detected (0-4) |
| 1 | 1 | position_class | uint8 | Position enum (0=unknown, 1=standing, 2=sitting, 3=lying, 4=falling, 5=fallen, 6=absent) |
| 2 | 4 | fall_probability | float32 | Fall probability 0.0-1.0 |
| 6 | 4 | movement_index | float32 | Movement level 0.0-1.0 |
| 10 | 4 | distance_m | float32 | Distance to primary person (meters) |
| 14 | 4 | velocity_ms | float32 | Velocity of primary person (m/s) |
| 18 | 2 | radar_timestamp | uint16 | Milliseconds since last frame |

#### ENV_DATA (0x02) — 17 bytes

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 4 | temperature_c | float32 | Temperature in Celsius |
| 4 | 4 | humidity_pct | float32 | Relative humidity % |
| 8 | 4 | pressure_hpa | float32 | Atmospheric pressure hPa |
| 12 | 4 | iaq_index | float32 | Indoor Air Quality index 0-500 |
| 16 | 1 | room_id | uint8 | Room identifier |

#### BED_VITALS (0x03) — 26 bytes

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 4 | heart_rate_bpm | float32 | Heart rate in BPM |
| 4 | 4 | breathing_rate | float32 | Breaths per minute |
| 8 | 4 | movement_index | float32 | Overall movement 0.0-1.0 |
| 12 | 1 | in_bed | uint8 | 0=empty, 1=occupied |
| 13 | 1 | sleep_phase | uint8 | 0=absent, 1=awake, 2=light, 3=deep, 4=REM |
| 14 | 4 | hr_confidence | float32 | Heart rate confidence 0.0-1.0 |
| 18 | 4 | br_confidence | float32 | Breathing confidence 0.0-1.0 |
| 22 | 4 | mattress_temp_c | float32 | Mattress temperature |

#### FALL_ALERT (0x07) — 14 bytes (CRITICAL — bypasses TDMA)

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 1 | room_id | uint8 | Room where fall detected |
| 1 | 1 | position_class | uint8 | Position at time of fall |
| 2 | 4 | fall_probability | float32 | Fall confidence score |
| 6 | 4 | impact_velocity | float32 | Estimated impact velocity m/s |
| 10 | 4 | timestamp | uint32 | UTC timestamp of detection |

#### PANIC_ALERT (0x08) — 8 bytes

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0 | 1 | tag_id | uint8 | Wearable tag identifier |
| 1 | 1 | battery_pct | uint8 | Battery percentage |
| 2 | 4 | timestamp | uint32 | UTC timestamp |
| 6 | 1 | button_hold_time | uint8 | Seconds button was held |

## BLE Protocol (Wearable Tags)

### GATT Service: HearthKeep Tag (0xHEAR)

| Characteristic | UUID | Properties | Size | Description |
|---------------|------|-----------|------|-------------|
| Panic Status | 0xHE01 | Notify, Read | 1 byte | 0=idle, 1=panic, 2=cancel |
| Fall Status | 0xHE02 | Notify, Read | 1 byte | 0=normal, 1=fall_detected |
| Battery Level | 0xHE03 | Notify, Read | 1 byte | 0-100% |
| Tag Config | 0xHE04 | Write | 4 bytes | Sensitivity, LED, buzzer |

### Connectionless Advertising

Tags advertise every 2 seconds in connectionless mode:

```
PDU: ADV_NONCONN_IND
Interval: 2000ms
TX Power: +4 dBm

Advertising Data:
  Flags: BR/EDR Not Supported | General Discoverable
  Complete Local Name: "HK-TAG-XXXX" (4 hex digits = tag ID)
  Manufacturer Data:
    Company ID: 0x484B ("HK")
    Data:
      [0]: Tag ID (2 bytes)
      [2]: Battery % (1 byte)
      [3]: Panic Status (1 byte)
      [4]: Fall Status (1 byte)
```

## WiFi/MQTT Bridge Protocol

The hub publishes all sensor data to an MQTT broker for cloud processing.

### Topics

```
hearethkeep/sensors/radar/<node_id>     — Radar data (every 1s when active)
hearethkeep/sensors/env/<node_id>        — Environment data (every 30s)
hearethkeep/sensors/vitals/<node_id>     — Bed vitals (every 30s when in bed)
hearethkeep/alerts/fall/<node_id>         — Fall alerts (immediate)
hearethkeep/alerts/panic/<tag_id>         — Panic alerts (immediate)
hearethkeep/status/<node_id>             — Heartbeat (every 60s)
hearethkeep/commands/<node_id>            — Commands from cloud to node
hearethkeep/ota/<node_type>              — OTA firmware updates
```

### QoS Levels

| Topic | QoS | Reason |
|-------|-----|--------|
| alerts/fall/* | 1 | Critical — must be delivered |
| alerts/panic/* | 1 | Critical — must be delivered |
| sensors/radar/* | 0 | High volume, latest value is most important |
| sensors/vitals/* | 1 | Important for health monitoring |
| sensors/env/* | 0 | Low priority environment data |
| status/* | 0 | Heartbeat data |
| commands/* | 1 | Commands must be delivered |
| ota/* | 1 | Firmware must be delivered completely |

### Security

- MQTT over TLS (port 8883)
- Username/password authentication per home
- Each hub has unique client certificate
- Topics scoped per home: `hearethkeep/<home_id>/sensors/...`