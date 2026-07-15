# LawnSync — Protocol Specification

## Physical Layer

| Parameter | Value |
|-----------|-------|
| Band | 868 MHz (EU) / 915 MHz (US) |
| Modulation | LoRa (Semtech SX1262/SX1276) |
| Spreading Factor | SF9 (adaptive: SF7–SF11) |
| Bandwidth | 125 kHz (adaptive: 62.5–500 kHz) |
| Coding Rate | 4/5 (configurable) |
| Preamble | 8 symbols |
| TX Power | +22 dBm (SX1262), +20 dBm (SX1276) |
| Range | 300 m LOS (SF7), 2 km+ (SF11 + mesh relay) |

## TDMA Mesh MAC Layer

### Frame Structure
```
┌──────────────────────────────────────────────────────────────┐
│ Frame (1600 ms)                                              │
├────────┬────────┬────────┬────────┬─────────────────┬───────┤
│Slot 0  │Slot 1  │Slot 2  │Slot 3  │ ...             │Slot 31│
│Hub     │Node 1  │Node 2  │Node 3  │                 │Relay  │
│Beacon  │Telemetry│Telemetry│Telemetry│              │       │
└────────┴────────┴────────┴────────┴─────────────────┴───────┘
  50ms     50ms     50ms     50ms                       50ms
```

- **Slot 0:** Hub beacon (time sync + slot assignments)
- **Slots 1–30:** Node telemetry (assigned by hub during JOIN)
- **Slot 31:** Relay slot (for out-of-range nodes)

### Join Procedure
1. Node powers on → sends JOIN_REQ (broadcast, any time)
2. Hub receives → assigns node ID + TDMA slot → sends JOIN_ACK
3. Node confirms → enters normal TDMA schedule

### Time Synchronization
- Hub broadcasts TIME_SYNC every 60 seconds (slot 0)
- Nodes adjust local clock based on beacon timestamp
- ±1 ms accuracy sufficient for 50 ms slots

## Message Format

### Binary Frame
```
┌──────────┬──────────┬──────────┬──────────┬───────────┬──────────┬──────────┐
│ Sync (2) │ Src (1)  │ Dst (1)  │ MsgType  │ MsgId (2) │ Payload  │ CRC16 (2)│
│ 0xA5 0x5A│ NodeID  │ 0xFF=All│  (1)     │           │ (N)      │          │
└──────────┴──────────┴──────────┴──────────┴───────────┴──────────┴──────────┘
  2 bytes    1 byte     1 byte    1 byte    2 bytes   N bytes   2 bytes
```

- **Sync:** Always 0xA5 0x5A (word alignment)
- **Src:** Source node ID (0 = hub, 1–31 = nodes, 0xFF = unassigned)
- **Dst:** Destination (0xFF = broadcast)
- **MsgType:** Message type (see below)
- **MsgId:** 16-bit sequence number (for deduplication and ACK)
- **Payload:** 0–240 bytes (type-specific)
- **CRC16:** CRC-16-CCITT over all preceding bytes (poly 0x1021, init 0xFFFF)

### Encryption
- AES-128-CCM (Counter with CBC-MAC)
- Network key: 16 bytes, shared by all nodes (provisioned at manufacturing)
- Session key: Derived per-node during JOIN (HKDF from network key + node ID)
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
| OTA_BLOCK | 0x07 | Hub→Node | 136 | Firmware chunk (128B + 8B header) |
| OTA_ACK | 0x08 | Node→Hub | 4 | Firmware chunk acknowledgment |
| HEARTBEAT | 0x09 | Node→Hub | 8 | Alive + battery + RSSI |
| MESH_RELAY | 0x0A | Node→Node | 240 | Forwarded message for out-of-range peer |
| SCAN_RESULT | 0x0B | Scanner→Hub | 14 | Disease/weed detection result |
| TIME_SYNC | 0x0C | Hub→All | 4 | Epoch time for TDMA sync |
| CONFIG | 0x0D | Hub→Node | 240 | Configuration update |
| CONFIG_ACK | 0x0E | Node→Hub | 4 | Config applied confirmation |

## Payload Formats

### JOIN_REQ Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Node type | 0=hub, 1=soil, 2=sprinkler, 3=weather, 4=scanner |
| 1 | 1 | Protocol version | Currently 1 |
| 2 | 1 | Capabilities | Bitmask (bit0=moisture, bit1=ph, bit2=npk, ...) |
| 3 | 1 | Battery (×0.01V) | 0–255 → 0.00–2.55V (offset 2.0V) |
| 4 | 1 | FW version major | |
| 5 | 1 | FW version minor | |

### JOIN_ACK Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Assigned node ID | 1–31 |
| 1 | 1 | Assigned TDMA slot | 1–30 |

### TELEMETRY — Soil Node (Sub-type 0x01)
| Offset | Size | Field | Unit | Range |
|--------|------|-------|------|-------|
| 0 | 1 | Sub-type | | 0x01 |
| 1 | 1 | Battery | ×0.01 V | 200–365 |
| 2 | 2 | Soil moisture | ×0.01 % | 0–10000 |
| 4 | 2 | Soil temp | ×0.1 °C (signed) | -400–850 |
| 6 | 1 | pH | ×0.1 | 30–90 |
| 7 | 2 | Nitrogen | ×0.1 mg/kg | 0–65535 |
| 9 | 2 | Phosphorus | ×0.1 mg/kg | 0–65535 |
| 11 | 2 | Potassium | ×0.1 mg/kg | 0–65535 |
| 13 | 2 | Light | ×1 lux | 0–120000 |
| 15 | 1 | Solar voltage | ×0.1 V | 0–25 |
| 16 | 1 | RSSI | dBm (signed) | -127–0 |
| 17 | 2 | Sequence | counter | |

**Total: 19 bytes**

### TELEMETRY — Weather Station (Sub-type 0x02)
| Offset | Size | Field | Unit | Range |
|--------|------|-------|------|-------|
| 0 | 1 | Sub-type | | 0x02 |
| 1 | 1 | Battery | ×0.01 V | |
| 2 | 2 | Temperature | ×0.1 °C (signed) | |
| 4 | 2 | Humidity | ×0.1 % | |
| 6 | 2 | Pressure | ×0.1 hPa (offset 800) | |
| 8 | 2 | Wind speed | ×0.1 m/s | |
| 10 | 2 | Wind direction | ×1 degree | 0–360 |
| 12 | 2 | Rain tips | ×0.2 mm/tip | |
| 14 | 2 | Solar irradiance | ×1 W/m² | |
| 16 | 1 | UV index | ×0.1 | |
| 17 | 1 | RSSI | dBm (signed) | |

**Total: 18 bytes**

### TELEMETRY — Sprinkler Controller (Sub-type 0x03)
| Offset | Size | Field | Unit | Range |
|--------|------|-------|------|-------|
| 0 | 1 | Sub-type | | 0x03 |
| 1 | 1 | Active zone | | 0=none, 1–8 |
| 2 | 2 | Flow rate | ×0.1 L/min | |
| 4 | 4 | Total flow | ×0.1 L (cumulative) | |
| 8 | 2 | Pressure | ×0.1 kPa | |
| 10 | 1 | Rain detected | | 0/1 |
| 11 | 1 | Valve status | bitmask | bit0–7 = zone 1–8 |
| 12 | 1 | RSSI | dBm (signed) | |

**Total: 13 bytes**

### COMMAND Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Command type | See below |
| 1+ | N | Command data | Type-specific |

**Command Types:**
| Code | Name | Data | Notes |
|------|------|------|-------|
| 0x01 | VALVE_OPEN | zone(1) + duration_s(2) | Open zone for N seconds |
| 0x02 | VALVE_CLOSE | zone(1) | Close specific zone |
| 0x03 | SCAN_CAPTURE | none | Trigger scanner capture |
| 0x04 | SET_CONFIG | config_blob(N) | Update node configuration |
| 0x05 | REBOOT | none | Reboot node |
| 0x06 | CALIBRATE | cal_type(1) + data(N) | Start calibration |

### SCAN_RESULT Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Disease class | 0–14 (see DiseaseNet classes) |
| 1 | 1 | Confidence | ×0.01 (0–100) |
| 2 | 2 | Avg NDVI | ×100 (signed, -100–100) |
| 4 | 1 | Weed coverage | % (0–100) |
| 5 | 1 | Dominant weed | 0–8 (see WeedSeg classes) |
| 6 | 4 | GPS latitude | ×1e-6 degrees (signed int32) |
| 10 | 4 | GPS longitude | ×1e-6 degrees (signed int32) |

**Total: 14 bytes**

### ALERT Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Alert type | See below |
| 1 | 1 | Severity | 1=info, 2=warning, 3=critical |
| 2+ | N | Alert data | Type-specific |

**Alert Types:**
| Code | Name | Data |
|------|------|------|
| 0x01 | LOW_BATTERY | battery_mv(1) |
| 0x02 | LOW_MOISTURE | moisture(2), threshold(2) |
| 0x03 | HIGH_MOISTURE | moisture(2), threshold(2) |
| 0x04 | LEAK_DETECTED | flow_rate(2) |
| 0x05 | OVERPRESSURE | pressure(2) |
| 0x06 | FREEZE | temp(2) |
| 0x07 | VALVE_FAULT | zone(1), flow(2) |
| 0x08 | DISEASE | disease_class(1), confidence(1) |
| 0x09 | TAMPER | status(1) |
| 0x0A | NODE_OFFLINE | node_id(1), last_seen_s(4) |

### OTA_BLOCK Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 4 | Offset | Firmware offset in bytes |
| 4 | 1 | Total chunks | Total number of chunks |
| 5 | 1 | Chunk index | Current chunk index (0-based) |
| 6 | 1 | Chunk size | Payload size in this block (0–128) |
| 7 | 1 | CRC8 | CRC-8 of payload data |
| 8 | 128 | Data | Firmware binary chunk |

**Total: 136 bytes (max)**

### HEARTBEAT Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | Battery | ×0.01 V |
| 1 | 1 | RSSI | dBm (signed) |
| 2 | 4 | Uptime | Seconds since boot |
| 6 | 1 | Errors | Error counter |
| 7 | 1 | Temperature | ×1 °C (MCU internal) |

**Total: 8 bytes**

### TIME_SYNC Payload
| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 4 | Epoch time | Unix timestamp (UTC) |

**Total: 4 bytes**

## Retransmission & Reliability

- **Telemetry:** Fire-and-forget (no ACK). Hub may send CONFIG if data missing.
- **Commands:** Hub expects CMD_ACK within 2 seconds, retries 3×.
- **OTA:** Each block requires OTA_ACK with matching CRC. Retry 5× per block.
- **Deduplication:** Nodes track MsgId (16-bit) — discard duplicates within 60s window.
- **Mesh relay:** If hub doesn't ACK a node's message within 2 slots, relay nodes
  in slot 31 will retransmit on behalf of the out-of-range node.