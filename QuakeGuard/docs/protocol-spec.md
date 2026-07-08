# QuakeGuard — Protocol Specification

## Physical Layer

- **Frequency**: 868 MHz (EU/Asia ISM band)
- **Modulation**: GFSK (CC1101)
- **Data rate**: 38.4 kBaud
- **Channel spacing**: 200 kHz
- **TX power**: 10 dBm (adjustable 0–10 dBm)
- **Range**: 100–300 m in buildings (penetrates walls/floors)

## Frame Format

```
┌──────────┬──────────┬──────────┬──────────────────┬───────────┬──────────┐
│ Preamble  │ Sync     │ Length   │ Msg Type (1B)    │ Payload   │ CRC16    │
│ (4 B)     │ (2 B)    │ (1 B)    │ + Src (1B)       │ (N B)     │ (2 B)    │
│ 0xAA...   │ 0x2DD4   │          │ + Dst (1B)       │           │          │
│           │          │          │ + Seq  (1B)      │           │          │
└──────────┴──────────┴──────────┴──────────────────┴───────────┴──────────┘
   4 B         2 B        1 B           4 B            0–128 B      2 B
```

- **Preamble**: 4 bytes of 0xAA (CC1101 hardware sync)
- **Sync word**: 0x2DD4 (CC1101 SYNC1/SYNC0 registers)
- **Length**: 1 byte (header + payload, not CRC)
- **Header**: 4 bytes (msg_type, src_addr, dst_addr, seq_num)
- **Payload**: 0–128 bytes
- **CRC**: 16-bit CRC-CCITT over (Length + Header + Payload)

## Addressing

| Address Range | Node Type |
|--------------|-----------|
| 0x00 | Hub |
| 0x10–0x1F | Floor Nodes (up to 16) |
| 0x20 | Shutoff Controller |
| 0x30–0x3F | Structural Tags (up to 16) |
| 0xFF | Broadcast |

## TDMA Mesh

- **Frame period**: 500 ms
- **Time slot**: 50 ms per node
- **Max nodes**: 10 (500 ms / 50 ms)
- **Priority preemption**: Seismic events interrupt normal TDMA schedule
- **Heartbeat slot**: Each node transmits heartbeat in its assigned slot

### Slot Assignment

```
0–50 ms:    Hub (broadcast / commands)
50–100 ms:  Floor Node 0 (0x10)
100–150 ms: Floor Node 1 (0x11)
150–200 ms: Floor Node 2 (0x12)
200–250 ms: Floor Node 3 (0x13)
250–300 ms: Shutoff Controller (0x20)
300–350 ms: Structural Tag 0 (0x30)
350–400 ms: Structural Tag 1 (0x31)
400–450 ms: Reserved (OTA / config)
450–500 ms: Contention (event preemption)
```

## Message Types

### 0x01 — HEARTBEAT (Node→Hub, every 60 s)
```c
typedef struct {
    uint8_t  battery_pct;      // 0–100
    int16_t  temperature_c;   // °C × 10
    uint8_t  status_flags;     // bit 0: online, bit 1: fault
    uint8_t  uptime_hours;
    uint16_t rssi_db;          // 0xFFFF = N/A
} heartbeat_payload_t;  // 8 bytes
```

### 0x02 — SEISMIC_CANDIDATE (Floor→Hub, priority)
```c
typedef struct {
    uint8_t  chunk_id;
    uint8_t  total_chunks;
    uint8_t  axis_flags;       // bit 0: X, 1: Y, 2: Z
    uint8_t  sample_rate_khz;  // 1 = 1000 Hz
    uint8_t  data[120];        // compressed waveform chunk
} seismic_payload_t;  // 4 + up to 120 bytes
```

Waveform compression: delta encoding + RLE for zero-runs.
Full 2 s × 3-axis × 1000 Hz × 2 bytes = 12 KB → compressed ~40–80 bytes.

### 0x03 — SEISMIC_CONFIRMED (Hub→All, broadcast)
```c
typedef struct {
    uint32_t timestamp_utc;
    uint8_t  severity;         // 0–4
    uint8_t  magnitude_x10;    // Mw × 10
    uint16_t epicenter_dist_km;
    uint8_t  actions_taken;   // QG_ACT_* bitmask
    uint8_t  node_count;
    uint8_t  reserved[5];
} seismic_confirmed_payload_t;  // 16 bytes
```

### 0x04 — SHUTOFF_NOW (Hub→Shutoff, ACK required)
```c
typedef struct {
    uint8_t  action_flags;    // QG_ACT_* bitmask
    uint8_t  urgency;         // 0=test, 1=normal, 2=immediate
    uint16_t event_id;
} shutoff_now_payload_t;  // 4 bytes
```

### 0x05 — SHUTOFF_ACK (Shutoff→Hub, within 1 s)
```c
typedef struct {
    uint8_t  gas_valve_closed;
    uint8_t  water_valve_closed;
    uint16_t h2_ppm;
    uint16_t ch4_ppm;
    int16_t  temperature_c;   // °C × 10
    uint8_t  relay_states;
} shutoff_ack_payload_t;  // 8 bytes
```

### 0x06 — STRUCT_POLL (Hub→StructTag)
```c
typedef struct {
    uint16_t event_id;
} struct_poll_payload_t;  // 2 bytes
```

### 0x07 — STRUCT_REPORT (StructTag→Hub)
```c
typedef struct {
    int32_t  strain_max_micro;
    int32_t  strain_mean_micro;
    int16_t  resonance_shift_hz;
    int16_t  peak_accel_mg;
    int16_t  temperature_c10;
    uint8_t  battery_pct;
    uint8_t  anomaly_score;    // 0–255
    uint8_t  fault_flags;
    uint8_t  reserved[3];
} struct_report_payload_t;  // 20 bytes
```

## Action Flags

| Flag | Value | Action |
|------|-------|--------|
| QG_ACT_GAS_VALVE | 0x01 | Close gas main |
| QG_ACT_WATER_VALVE | 0x02 | Close water main |
| QG_ACT_RELAY_1 | 0x04 | Elevator drop |
| QG_ACT_RELAY_2 | 0x08 | Awning retract |
| QG_ACT_RELAY_3 | 0x10 | Non-essential power cut |
| QG_ACT_RELAY_4 | 0x20 | Gas appliance cutoff |
| QG_ACT_ALL | 0x3F | All actions |

## Reliability

- **CRC16-CCITT**: All frames include CRC; corrupted frames are silently dropped.
- **ACK for SHUTOFF_NOW**: Hub retries 3× with 1 s timeout if no ACK from Shutoff.
- **Sequence numbers**: Each message has a rolling sequence number for deduplication.
- **Retransmission**: Seismic candidates are sent with best-effort (no retransmit) — the consensus algorithm handles missing candidates from individual nodes.

## Security

- **AES-128**: All Sub-GHz payloads encrypted with a shared network key (provisioned during pairing).
- **Ed25519 firmware signing**: Nodes refuse unsigned firmware updates.
- **TLS 1.3**: Cloud MQTT and REST API use TLS.