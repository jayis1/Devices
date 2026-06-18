# ThermoGrid — Mesh Protocol Specification

## Overview

ThermoGrid uses a custom TDMA (Time Division Multiple Access) protocol over
LoRa 915MHz (US) / 868MHz (EU) Sub-GHz radio for reliable, low-latency
communication between thermal nodes. The protocol is designed for:

- **Freeze protection priority:** Alerts override normal TDMA on SF12 (max range)
- **Dynamic frame:** TDMA frame grows with the number of enrolled nodes
- **Low power:** Room sensors achieve <8µA in STOP, comfort tags <15µA
- **Reliability:** CRC16 error detection + ACK/retransmit
- **Energy-aware:** Solar + TOU status broadcast to all nodes every frame

## Physical Layer

| Parameter | US (915MHz) | EU (868MHz) |
|-----------|-------------|-------------|
| Frequency | 915.0 MHz | 868.0 MHz |
| Modulation | LoRa | LoRa |
| Spreading Factor | SF7 (normal) | SF7 (normal) |
| | SF9 (commands) | SF9 (commands) |
| | SF12 (freeze alert) | SF12 (freeze alert) |
| Bandwidth | 125 kHz | 125 kHz |
| TX Power | +14 dBm (nodes) | +14 dBm (nodes) |
| | +20 dBm (hub) | +20 dBm (hub) |
| Range (indoor) | 30m | 30m |
| Range (SF9) | 100m | 100m |
| Range (SF12) | 300m+ | 300m+ |
| Coding Rate | 4/5 | 4/5 |
| Preamble | 8 symbols | 8 symbols |
| Sync Word | 0x5447 | 0x5447 |

## TDMA Frame Structure

```
Time →  0ms     100ms    200ms    ...   (N+1)×100ms   (N+2)×100ms
        ┌───────┬───────┬───────┬─── ───┬───────┬───────┐
        │ HUB   │SENSOR │SENSOR │  ...  │ACTUATR│ CTRL  │
        │ CMD   │  1    │  2    │       │ (all) │ ACK   │
        │Slot 0 │Slot 1 │Slot 2 │       │Slot N+1│Slot N+2│
        └───────┴───────┴───────┴─── ───┴───────┴───────┘

Total frame: (N+2) × 100ms  (N = number of enrolled sensors + actuators)
Slot 0: Hub broadcasts sync + zone setpoints + solar status + TOU schedule
Slots 1..N: Each room sensor and actuator uplinks telemetry
Slot N+1: Hub sends zone setpoints to actuators
Slot N+2: Control / ACK / retransmit / OTA
```

## Freeze Protection Override

```
When any room sensor reports temperature < 4°C:
  1. Sensor immediately broadcasts FREEZE_ALERT on SF12 (max range)
  2. Hub halts normal TDMA
  3. Hub forces ALL zone actuators to 100% valve open
  4. Hub activates boiler/heat-pump relay
  5. Hub forwards alert to cloud + mobile app (if WiFi available)
  6. Normal TDMA resumes after all rooms >6°C for 5 minutes

This works WITHOUT WiFi/cloud — freeze protection is a local mesh function.
The hub has battery backup to ensure this works during power outages.
```

## Window-Open Override

```
When room sensor detects window open (temp drop >1°C + air velocity spike):
  1. Sensor sends WINDOW_OPEN alert to hub
  2. Hub sends VALVE_CLOSE to that zone's actuator
  3. Zone conditioning paused for 10 min
  4. Alert: "Window open in <room> — heating paused"
  5. When sensor reports window closed (temp stabilizing), zone resumes
  6. If window open >30 min, app alert "Window still open"
```

## Packet Format

```
[ PREAMBLE(4) | SYNC(2) | LEN(1) | SRC_ID(1) | DST_ID(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2) ]

PREAMBLE: 0xAA 0xAA 0xAA 0xAA
SYNC:     0x5447 ("TG")
LEN:      payload length (0-50)
SRC_ID:   source node ID
DST_ID:   destination (0xFF = broadcast)
TYPE:     packet type (see below)
PAYLOAD:  type-specific struct (0-50 bytes)
CRC16:    CRC-16/CCITT over LEN+SRC+DST+TYPE+PAYLOAD
```

## Packet Types

| Type | Code | Payload | Direction |
|------|------|---------|-----------|
| SENSOR_DATA | 0x01 | sensor_data_payload_t (36B) | sensor → hub |
| ACTUATOR_DATA | 0x02 | actuator_data_payload_t (28B) | actuator → hub |
| COMFORT_DATA | 0x03 | comfort_data_payload_t (24B) | tag → hub |
| COMMAND | 0x04 | command_payload_t | hub → node |
| ACK | 0x05 | — | node → hub |
| OTA_BLOCK | 0x06 | firmware chunk | hub → node |
| CALIBRATION | 0x07 | sensor calibration | hub → sensor |
| FREEZE_ALERT | 0x08 | freeze_alert_payload_t (16B) | sensor → broadcast |
| WINDOW_OPEN | 0x09 | window_open_payload_t (12B) | sensor → hub |
| ZONE_SETPOINT | 0x0A | zone_setpoint_payload_t (16B) | hub → actuator |
| HEARTBEAT | 0x0B | system state | hub → broadcast |
| ENERGY_REPORT | 0x0C | energy_report_payload_t (20B) | actuator → hub |
| SOLAR_STATUS | 0x0D | solar_status_payload_t (12B) | hub → actuators |
| TOU_SCHEDULE | 0x0E | tou_schedule_payload_t (10B) | hub → actuators |
| COMFORT_VOTE | 0x0F | comfort_vote_payload_t (8B) | tag → hub |

## Node IDs

| ID Range | Node Type |
|----------|-----------|
| 0x00 | Hub |
| 0x10-0x7F | Sensors and actuators (assigned at enrollment) |
| 0x80-0x8F | Comfort tags (paired persons) |
| 0xFF | Broadcast |

See `firmware/common/mesh_protocol.h` for full struct definitions.