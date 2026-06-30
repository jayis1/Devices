# AsthmaSync — Communication Protocol Specification

## Version
1.0 (PROTO_VERSION = 0x10)

## Packet Format

All AsthmaSync communication uses a fixed-format packet:

```
┌─────────┬─────────┬─────────┬──────────┬──────────┬──────┬─────────────┬─────────┐
│ Magic   │ Version │ SrcType │ SrcID    │ MsgType  │ Seq  │ PayloadLen  │ CRC16   │
│ 1 byte  │ 1 byte  │ 1 byte  │ 2 bytes  │ 1 byte   │ 1 B  │ 2 bytes     │ 2 bytes │
└─────────┴─────────┴─────────┴──────────┴──────────┴──────┴─────────────┴─────────┘
```

**Total header: 10 bytes. Max payload: 128 bytes. Max packet: 138 bytes.**

### Fields

| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| Magic | 0 | 1 | Always 0xA5 — packet start marker |
| Version | 1 | 1 | Upper nibble = major, lower = minor (1.0 = 0x10) |
| SrcType | 2 | 1 | Node type (0x01=Hub, 0x02=AirSentinel, 0x03=InhalerTag, 0x04=WheezeBand) |
| SrcID | 3 | 2 | Node unique ID (last 2 bytes of MAC address) |
| MsgType | 5 | 1 | Message type (see below) |
| Seq | 6 | 1 | Sequence number (wraps from 255→0) |
| PayloadLen | 7 | 2 | Payload length in bytes (0-128) |
| CRC | 9 | 2 | CRC-16/CCITT (poly 0x1021, init 0xFFFF) over bytes 0-7 + payload |

### CRC Calculation

CRC-16/CCITT is computed over all bytes **except** the CRC field itself (bytes 0 through PayloadLen+8). The CRC field is set to 0x0000 during computation, then the result is written back.

## Message Types

| Type | Code | Direction | Description |
|------|------|-----------|-------------|
| JOIN_REQ | 0x01 | Node→Hub | Join mesh network |
| JOIN_ACK | 0x02 | Hub→Node | Slot assignment |
| TELEMETRY | 0x10 | Node→Hub | Periodic sensor data |
| EVENT | 0x11 | Node→Hub | Discrete event (wheeze, actuation, alert) |
| ALERT | 0x12 | Hub→Node | Alert notification |
| CONFIG | 0x20 | Hub→Node | Configuration update |
| ACK | 0x30 | Either | Reliable delivery acknowledgment |
| PING | 0x40 | Either | Keepalive |
| PONG | 0x41 | Either | Keepalive response |
| TIME_SYNC | 0x50 | Hub→Node | Epoch time synchronization |

## Telemetry Payload Format

Telemetry messages (MsgType=0x10) use a TLV (Type-Length-Value) scheme. The first byte of the payload is the TLV type:

| TLV Type | Code | Payload Size | Structure |
|----------|------|-------------|-----------|
| AIR_QUALITY | 0x01 | 16 bytes | air_quality_t |
| VITALS | 0x02 | 12 bytes | vitals_t |
| AUDIO_FEATURE | 0x03 | 48 bytes | audio_feature_t |
| ACTUATION | 0x04 | 8 bytes | actuation_t |
| BATTERY | 0x05 | 4 bytes | voltage (u16 mV) + level_pct (u8) + reserved |

### air_quality_t (16 bytes)

| Offset | Field | Type | Scale | Unit |
|--------|-------|------|-------|------|
| 0 | pm1_0 | u16 | ×10 | µg/m³ |
| 2 | pm2_5 | u16 | ×10 | µg/m³ |
| 4 | pm10 | u16 | ×10 | µg/m³ |
| 6 | voc_index | u16 | — | 0-500 (BME688 IAQ) |
| 8 | hcho_ppb | u16 | — | ppb (SGP40) |
| 10 | co2_ppm | u16 | — | ppm (SCD41) |
| 12 | temp_c | s16 | ×10 | °C |
| 14 | humidity | u16 | ×10 | %RH |

### vitals_t (12 bytes)

| Offset | Field | Type | Scale | Unit |
|--------|-------|------|-------|------|
| 0 | hr | u8 | — | bpm |
| 1 | spo2 | u8 | — | % |
| 2 | hrv_rmssd | u16 | ×10 | ms |
| 4 | skin_temp | s16 | ×10 | °C |
| 6 | activity | u8 | — | 0-3 |
| 7-11 | reserved | 5B | — | future use |

### audio_feature_t (48 bytes)

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0-31 | mel_bins[32] | u8[32] | 32 log-mel bins (last frame) |
| 32 | wheeze_prob | u8 | Pre-classifier probability 0-100 |
| 33 | snr_db | u8 | Signal-to-noise ratio |
| 34 | window_ms | u16 | Analysis window length |
| 36 | reserved | u16 | Future use |

### actuation_t (8 bytes)

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | type | u8 | 0=MDI, 1=DPI |
| 1 | confidence | u8 | 0-100% |
| 2 | peak_accel | s16 | ×1000 g |
| 4 | duration_ms | u16 | Event duration |
| 6 | battery_pct | u8 | 0-100% |
| 7 | reserved | u8 | — |

## Event Types

Events (MsgType=0x11) use event_payload_t (8 bytes):

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | event_type | u8 | See table below |
| 1 | severity | u8 | 0=info, 1=warning, 2=critical |
| 2 | zone | u8 | 0=green, 1=yellow, 2=red |
| 3 | reserved | u8 | — |
| 4 | timestamp | u32 | Epoch seconds |

### Event Type Codes

| Event | Code | Severity | Trigger Condition |
|-------|------|----------|-------------------|
| WHEEZE_DETECTED | 0x01 | 1 | Wheeze pre-classifier > 65% |
| ACTUATION | 0x02 | 1 | Inhaler actuation confirmed |
| SPO2_LOW | 0x03 | 2 | SpO₂ < 92% (Red Zone) |
| HRV_DROP | 0x04 | 1 | rmSSD < baseline - 20% |
| PM25_HIGH | 0x05 | 1 | PM2.5 > 35 µg/m³ |
| VOC_HIGH | 0x06 | 1 | VOC index > 400 |
| FALL | 0x07 | 2 | IMU fall detection |
| BUTTON_SOS | 0x08 | 2 | Manual SOS button press |

## TDMA Mesh (Sub-GHz)

### Superframe Structure

```
Slot 0        Slot 1       Slot 2       ...  Slot 7
┌──────────┬──────────┬──────────┬─...─┬──────────┐
│  Beacon   │  Node 1  │  Node 2  │     │  Node 7  │
│  (Hub TX) │  (Node TX)│  (Node TX)│     │  (Node TX)│
└──────────┴──────────┴──────────┴─...─┴──────────┘
← 200 ms →← 200 ms →← 200 ms →     ← 200 ms →
←─────────────── 2000 ms superframe ──────────────→
```

- Beacon (slot 0): Hub sends TIME_SYNC packet with timestamp + slot assignments
- Data slots (1-7): Assigned nodes transmit telemetry
- Guard time: 20ms per slot for clock drift compensation
- Mesh fallback: If a node can't reach the Hub directly, it relays through another node

## BLE GATT Service

### Service: AsthmaSync (0xA501)

| Characteristic | UUID | Properties | Description |
|---------------|------|------------|-------------|
| Telemetry | 0x2A01 | Read, Notify | Latest sensor data |
| Event | 0x2A03 | Notify | Real-time event stream |
| Command | 0x2A02 | Write | Config/control commands |

### CCC (Client Characteristic Configuration)
Both Telemetry and Event characteristics support CCC descriptors for enabling/disabling notifications.