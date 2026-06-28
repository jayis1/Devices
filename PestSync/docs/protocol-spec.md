# PestSync — Protocol Specification

## PestSync Protocol (PSP)

PSP is the custom protocol used over Sub-GHz 868 MHz LoRa for all inter-node communication.

### Physical Layer

| Parameter | Value |
|-----------|-------|
| Frequency | 868 MHz (EU ISM) |
| Modulation | LoRa (SX1262) |
| Bandwidth | 125 kHz |
| Spreading Factor | SF11 |
| Coding Rate | 4/5 |
| TX Power | 17 dBm (50 mW EU limit) |
| Preamble | 8 symbols |
| Sync Word | 0x3C |
| Range | ~500m outdoor, penetrates walls |

### Packet Format

```
┌──────────┬──────────┬──────────┬─────────┬──────────┬────────────────────┬──────────┐
│ Preamble │ Sync     │ Header   │ Src ID  │ Dst ID  │ Payload (encrypted) │  CRC16   │
│ 8 bytes  │ 4 bytes  │ 3 bytes  │ 2 bytes │ 2 bytes │ 0-128 bytes         │ 2 bytes  │
└──────────┴──────────┴──────────┴─────────┴──────────┴────────────────────┴──────────┘
```

### Header (3 bytes)

| Byte | Field |
|------|-------|
| 0 | Message type |
| 1 | Payload length |
| 2 | Sequence number |

### Timestamp (4 bytes)

Unix epoch seconds (set by Hub for TDMA sync).

### Encryption

AES-128-CCM:
- Key: 16 bytes (shared, per-household)
- Nonce: 12 bytes (SrcID[2] + SeqNum[1] + pad[9])
- MAC: 4 bytes (truncated CCM tag)

### CRC

CRC-16 CCITT (polynomial 0x1021, init 0xFFFF) over header + encrypted payload + MAC.

### Message Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| DATA | 0x01 | Node→Hub | Sensor/detection data |
| CMD | 0x02 | Hub→Node | Command |
| ACK | 0x03 | Hub→Node | Acknowledgment |
| JOIN | 0x04 | Node→Hub | Mesh join request |
| SYNC | 0x05 | Hub→All | TDMA slot sync beacon |
| ALERT | 0x06 | Node→Hub | High-priority alert |

### Commands

| Command | Value | Payload |
|---------|-------|---------|
| REBOOT | 0xF0 | — |
| OTA_BEGIN | 0xF1 | — |
| OTA_CHUNK | 0xF2 | offset[2] + data[16] |
| SET_RATE | 0x30 | interval_s[4] |
| TRIGGER_CAMERA | 0x40 | — |
| SET_DETER | 0x50 | mode[1] + band[1] + duration_s[2] |
| DETER_OFF | 0x51 | — |
| DETER_STROBE | 0x52 | — |
| DETER_DIFFUSE | 0x53 | — |
| RESET_TRAP | 0x60 | — |

### Node ID Ranges

| Range | Node Type |
|-------|-----------|
| 0x0001 | Hub |
| 0x0010-0x001F | Pest Sentinels |
| 0x0020-0x002F | Smart Traps |
| 0x0030-0x003F | Deterrent Nodes |
| 0xFFFF | Broadcast |

### TDMA Schedule

8-slot frame, 8000 ms total:

| Slot | Time (ms) | User |
|------|-----------|------|
| 0 | 0-1000 | Hub beacon (SYNC) |
| 1 | 1000-2000 | Sentinel #1 |
| 2 | 2000-3000 | Sentinel #2 |
| 3 | 3000-4000 | Trap #1 |
| 4 | 4000-5000 | Trap #2 |
| 5 | 5000-6000 | Deterrent #1 |
| 6 | 6000-7000 | Deterrent #2 |
| 7 | 7000-8000 | Hub command/ACK |

### Data Payloads

See `firmware/common/psp_protocol.h` for exact C struct definitions.

#### Sentinel Data (15 bytes)
```c
typedef struct {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    uint8_t  pest_class;       // 0-14, 255=none
    uint8_t  confidence;      // 0-100%
    uint16_t count_since_last;
    int16_t  thermal_max_c;   // x10
    uint8_t  ir_illumination;
    uint8_t  alerts;          // bitmask
} sentinel_data_t;
```

#### Trap Data (13 bytes)
```c
typedef struct {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    uint8_t  trap_status;     // 0=armed, 1=triggered, 2=needs_reset, 3=tampered
    uint16_t catch_weight_g;
    uint8_t  bait_level;      // 0-100%
    uint8_t  catch_class;    // 0=mouse, 1=rat, 2=insect, 3=false, 255=unknown
    uint8_t  alerts;
} trap_data_t;
```

#### Deterrent Data (17 bytes)
```c
typedef struct {
    uint16_t node_id;
    uint32_t uptime_s;
    uint8_t  battery_pct;
    uint8_t  ultrasonic_active;
    uint8_t  strobe_active;
    uint8_t  diffuser_active;
    uint8_t  oil_level;       // 0-100%
    uint32_t total_ultrasonic_s;
    uint16_t diffuser_doses;
    uint8_t  alerts;
} deterrent_data_t;
```

### BLE GATT (Mobile App ↔ Hub)

| Characteristic | UUID Suffix | Access | Description |
|---------------|-------------|--------|-------------|
| Read | 0x50E1 | READ | System snapshot (JSON) |
| Write | 0x50E2 | WRITE | Commands (JSON) |
| Notify | 0x50E3 | NOTIFY | Real-time alerts (JSON) |

Service UUID: `000050E0-0000-1000-8000-00805F9B34FB`