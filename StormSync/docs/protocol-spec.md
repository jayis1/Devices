# StormSync — Protocol Specification

## Physical Layer

| Parameter | Value |
|-----------|-------|
| Band | 868 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa (Semtech SX1262) |
| Spreading Factor | SF9 (adaptive: SF7–SF11) |
| Bandwidth | 125 kHz (adaptive) |
| Coding Rate | 4/5 (configurable) |
| Preamble | 8 symbols |
| TX Power | +22 dBm |
| Range | 300 m LOS (SF7), 2 km+ (SF11 + mesh relay) |

## TDMA Mesh MAC Layer

### Frame Structure
```
┌──────────────────────────────────────────────────────────────┐
│ Frame (1200 ms)                                              │
├────────┬────────┬────────┬────────┬─────────────────┬───────┤
│Slot 0  │Slot 1  │Slot 2  │Slot 3  │ ...             │Slot 23│
│Hub     │Sump    │Soil 1  │Soil 2  │                 │Relay  │
│Beacon  │Telemetry│Telemetry│Telemetry│              │       │
└────────┴────────┴────────┴────────┴─────────────────┴───────┘
  50ms     50ms     50ms     50ms                       50ms
```

- **Slot 0:** Hub beacon (time sync + slot assignments + flood status)
- **Slots 1–22:** Node telemetry (assigned by hub during JOIN)
- **Slot 23:** Relay slot (for out-of-range nodes)

## Message Format

### Binary Frame
```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0x5C 0xC5│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
  2 bytes    1 byte     1 byte    1 byte    2 bytes   N bytes   2 bytes
```

- **Sync:** Always 0x5C 0xC5
- **Src:** Source node ID (0 = hub, 1–22 = nodes, 0xFF = unassigned)
- **Dst:** Destination (0xFF = broadcast)
- **MsgType:** Message type
- **MsgId:** 16-bit sequence number
- **Payload:** 0–240 bytes
- **CRC16:** CRC-16-CCITT (poly 0x1021, init 0xFFFF)

### Encryption
- AES-128-CCM
- Network key: 16 bytes, shared (provisioned at manufacturing)
- Session key: Derived per-node during JOIN (HKDF)
- Nonce: 13 bytes (Src:1 + MsgId:2 + counter:10)

## Message Types

| Type | Code | Direction | Max Payload | Description |
|------|------|-----------|-------------|-------------|
| JOIN_REQ | 0x01 | Node→Hub | 8 | Join network request |
| JOIN_ACK | 0x02 | Hub→Node | 4 | Join response (ID + slot) |
| TELEMETRY | 0x03 | Node→Hub | 240 | Sensor data |
| COMMAND | 0x04 | Hub→Node | 240 | Actuator command |
| CMD_ACK | 0x05 | Node→Hub | 4 | Command acknowledgment |
| ALERT | 0x06 | Node→Hub | 240 | Threshold breach / fault |
| OTA_BLOCK | 0x07 | Hub→Node | 136 | Firmware chunk |
| OTA_ACK | 0x08 | Node→Hub | 4 | Firmware chunk acknowledgment |
| HEARTBEAT | 0x09 | Node→Hub | 8 | Alive + battery + RSSI |
| MESH_RELAY | 0x0A | Node→Node | 240 | Forwarded message |
| FLOOD_STATUS | 0x0B | Hub→All | 6 | Flood risk level + score |
| TIME_SYNC | 0x0C | Hub→All | 4 | Epoch time for TDMA sync |
| CONFIG | 0x0D | Hub→Node | 240 | Configuration update |
| CONFIG_ACK | 0x0E | Node→Hub | 4 | Config applied confirmation |

## Payload Formats

### JOIN_REQ Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Node type | 0=hub, 1=sump, 2=soil, 3=weather, 4=actuator |
| 1 | 1 | Protocol version | Currently 1 |
| 2 | 1 | Capabilities | Bitmask |
| 3 | 1 | Battery (×0.01V) | 0–255 (offset 2.0V) |
| 4 | 1 | FW version major | |
| 5 | 1 | FW version minor | |

### TELEMETRY — Sump Sentinel (Sub-type 0x01, 19 bytes)
| Offset | Size | Field | Unit |
|--------|------|-------|------|
| 0 | 1 | Sub-type | 0x01 |
| 1 | 1 | Battery | ×0.1 V |
| 2 | 2 | Water level | ×0.1 cm (0–4500) |
| 4 | 2 | Pump current | ×0.01 A (0–3000) |
| 6 | 1 | Pump status | 0=off, 1=running, 2=fault |
| 7 | 2 | Flow rate | ×0.1 L/min |
| 9 | 2 | Water temp | ×0.1 °C (signed) |
| 11 | 2 | Vibration RMS | ×0.001 g |
| 13 | 2 | Vibration peak | ×0.001 g |
| 15 | 1 | Mains power | 0=lost, 1=ok |
| 16 | 2 | Pump runtime today | ×1 minute |
| 18 | 1 | RSSI | dBm (signed) |

### TELEMETRY — Soil Probe (Sub-type 0x02, 15 bytes)
| Offset | Size | Field | Unit |
|--------|------|-------|------|
| 0 | 1 | Sub-type | 0x02 |
| 1 | 1 | Battery | ×0.01 V |
| 2 | 2 | Moisture 15cm | ×0.01 % |
| 4 | 2 | Moisture 45cm | ×0.01 % |
| 6 | 2 | Moisture 90cm | ×0.01 % |
| 8 | 2 | Pore pressure | ×0.1 kPa (signed, offset 500) |
| 10 | 1 | Temp 15cm | ×1 °C (signed) |
| 11 | 1 | Temp 45cm | ×1 °C |
| 12 | 1 | Temp 90cm | ×1 °C |
| 13 | 1 | Solar voltage | ×0.1 V |
| 14 | 1 | RSSI | dBm (signed) |

### TELEMETRY — Weather (Sub-type 0x03, 16 bytes)
| Offset | Size | Field | Unit |
|--------|------|-------|------|
| 0 | 1 | Sub-type | 0x03 |
| 1 | 1 | Battery | ×0.01 V |
| 2 | 2 | Temperature | ×0.1 °C |
| 4 | 2 | Humidity | ×0.1 % |
| 6 | 2 | Pressure | ×0.1 hPa (offset 800) |
| 8 | 2 | Wind speed | ×0.1 m/s |
| 10 | 2 | Wind direction | ×1 degree |
| 12 | 2 | Rain tips | ×0.2 mm/tip |
| 14 | 1 | Pressure trend | 0=steady, 1=rising, 2=falling |
| 15 | 1 | RSSI | dBm (signed) |

### TELEMETRY — Actuator (Sub-type 0x04, 9 bytes)
| Offset | Size | Field | Unit |
|--------|------|-------|------|
| 0 | 1 | Sub-type | 0x04 |
| 1 | 1 | Battery | ×0.1 V |
| 2 | 1 | Valve status | 0=open, 1=closed, 2=moving |
| 3 | 1 | Pump relay | 0=off, 1=on |
| 4 | 1 | Float switch | 0=normal, 1=high |
| 5 | 1 | Mains | 0=lost, 1=ok |
| 6 | 1 | Alarm | 0=silent, 1=active |
| 7 | 1 | Battery health | ×1 % |
| 8 | 1 | RSSI | dBm (signed) |

### FLOOD_STATUS Payload (6 bytes)
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Risk level | 0=low, 1=moderate, 2=high, 3=critical |
| 1 | 1 | Score | 0–100 |
| 2 | 2 | Predicted level | ×0.1 mm |
| 4 | 2 | Time to flood | ×1 minute (0 = no flood predicted) |

### COMMAND Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Command type | See below |
| 1+ | N | Command data | Type-specific |

**Command Types:**
| Code | Name | Data | Notes |
|------|------|------|-------|
| 0x01 | VALVE_CLOSE | none | Close backflow preventer |
| 0x02 | VALVE_OPEN | none | Open backflow preventer |
| 0x03 | PUMP_ON | none | Activate backup pump |
| 0x04 | PUMP_OFF | none | Deactivate backup pump |
| 0x05 | ALARM_ON | none | Activate siren |
| 0x06 | ALARM_OFF | none | Silence siren |
| 0x07 | STORM_MODE | none | Enter storm mode (increased sampling) |
| 0x08 | NORMAL_MODE | none | Return to normal mode |
| 0x09 | SET_CONFIG | config_blob(N) | Update configuration |
| 0x0A | REBOOT | none | Reboot node |
| 0x0B | CALIBRATE | cal_type(1) + data(N) | Start calibration |

### ALERT Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Alert type | See below |
| 1 | 1 | Severity | 1=info, 2=warning, 3=critical |
| 2+ | N | Alert data | Type-specific |

**Alert Types:**
| Code | Name | Data |
|------|------|------|
| 0x01 | LOW_BATTERY | battery_v(1) |
| 0x02 | HIGH_WATER | level_mm(2) |
| 0x03 | CRITICAL_WATER | level_mm(2) |
| 0x04 | PUMP_FAULT | status(1) |
| 0x05 | PUMP_OVERLOAD | current(2) |
| 0x06 | PUMP_DEGRADATION | class(1), confidence(1) |
| 0x07 | POWER_OUTAGE | battery_v(1) |
| 0x08 | VALVE_FAULT | status(1) |
| 0x09 | FLOAT_TRIGGER | state(1) |
| 0x0A | NODE_OFFLINE | node_id(1), last_seen_s(4) |
| 0x0B | STORM_IMMINENT | score(1), time_to_flood(2) |
| 0x0C | SENSOR_ANOMALY | sensor_id(1), score(1) |

## Retransmission & Reliability

- **Telemetry:** Fire-and-forget (no ACK). Hub may send CONFIG if data missing.
- **Commands:** Hub expects CMD_ACK within 2 seconds, retries 3×.
- **FLOOD_STATUS:** Broadcast every frame during storm mode (no ACK).
- **OTA:** Each block requires OTA_ACK with matching CRC. Retry 5× per block.
- **Deduplication:** Nodes track MsgId — discard duplicates within 60s window.
- **Mesh relay:** If hub doesn't ACK, relay nodes in slot 23 retransmit.