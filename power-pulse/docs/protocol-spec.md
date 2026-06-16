# PowerPulse Wireless Protocol Specification

## Frame Format

All PowerPulse nodes communicate using a custom binary frame format over Sub-GHz (868 MHz) radio. The protocol is designed for reliability, low overhead, and ease of parsing on resource-constrained MCUs.

### Frame Structure

```
┌──────┬──────┬──────┬──────┬──────┬──────┬───────────┬──────┐
│ SOF  │ LEN  │ SRC  │ DST  │ TYPE │ SEQ  │ PAYLOAD   │ CRC  │
│ 1B   │ 1B   │ 2B   │ 2B   │ 1B   │ 2B   │ 0-200B    │ 2B   │
└──────┴──────┴──────┴──────┴──────┴──────┴───────────┴──────┘
```

| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| SOF | 0 | 1 | Start of frame marker: `0xAA` |
| LEN | 1 | 1 | Payload length (0-200 bytes) |
| SRC | 2 | 2 | Source node address (little-endian) |
| DST | 4 | 2 | Destination address (little-endian). `0xFFFF` = broadcast |
| TYPE | 6 | 1 | Message type identifier |
| SEQ | 7 | 2 | Sequence number (little-endian, wraps at 65535) |
| PAYLOAD | 9 | LEN | Message-specific data |
| CRC | 9+LEN | 2 | CRC16-CCITT over header + payload |

**Total frame size**: 9 + LEN + 2 = 11 to 211 bytes

## Addressing

| Node Type | Address Range | Example |
|-----------|---------------|---------|
| Hub | `0x0001` | Always `0x0001` |
| Circuit Monitor | `0x0100` + panel_id (0-15) | `0x0100`, `0x0101`, ... |
| Appliance Tag | `0x0200` + tag_id (0-255) | `0x0200`, `0x0201`, ... |
| Solar Node | `0x0300` + node_id (0-15) | `0x0300`, `0x0301`, ... |
| Broadcast | `0xFFFF` | All nodes |

## Message Types

| Type | ID | Direction | Description |
|------|----|-----------|-------------|
| HEARTBEAT | 0x01 | Any → Hub/Broadcast | Node alive, battery, uptime |
| CIRCUIT_DATA | 0x02 | CM → Hub | Per-circuit power measurements |
| ARC_FAULT_ALERT | 0x03 | CM → Hub | Arc fault detection (critical) |
| APPLIANCE_DATA | 0x04 | Tag → Hub | Appliance energy data |
| APPLIANCE_CMD | 0x05 | Hub → Tag | Relay control command |
| SOLAR_DATA | 0x06 | Solar → Hub | Solar production data |
| SOLAR_CMD | 0x07 | Hub → Solar | MPPT control command |
| CALIBRATION | 0x08 | Hub → Any | Calibration parameters |
| OTA_UPDATE | 0x09 | Hub → Any | Firmware update chunk |
| OVERLOAD_ALERT | 0x0A | CM → Hub | Overcurrent detected |
| TIME_SYNC | 0x0B | Hub → Broadcast | Unix timestamp |
| ACK | 0xFF | Any → Any | Acknowledgment |

## Payload Definitions

### HEARTBEAT (0x01)
```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│node_type │battery_% │uptime_min│num_chnls │fw_version│signal_rssi│  flags   │
│  1B      │  1B      │  2B      │  1B      │  1B      │  1B      │  1B      │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
Total: 8 bytes
```

- `node_type`: 0x01=hub, 0x02=circuit_monitor, 0x03=appliance_tag, 0x04=solar
- `battery_%`: 0-100 (255 = mains powered)
- `flags`: bit0=error, bit1=calibrated, bit2=sd_card_present

### CIRCUIT_DATA (0x02)
```
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│voltage_mv│freq_cph  │num_active│circuit_ms│ readings │
│  2B      │  2B      │  1B      │  1B      │ variable │
└──────────┴──────────┴──────────┴──────────┴──────────┘
```

Each reading (8 bytes):
```
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│circuit_id│current_ma│ power_w  │power_fact│energy_wh │
│  1B      │  2B      │  2B      │  2B      │  2B      │
└──────────┴──────────┴──────────┴──────────┴──────────┘
```

Max readings per frame: `(200 - 5) / 8 = 24` (more than enough for 16 circuits)

### ARC_FAULT_ALERT (0x03)
```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│circuit_id│confidence│ arc_type │timestamp │ duration │severity  │
│  1B      │  1B      │  1B      │  4B      │  2B      │  1B      │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
Total: 10 bytes
```

- `arc_type`: 0=series, 1=parallel, 2=glowing contact
- `severity`: 1=low, 2=medium, 3=high, 4=critical

### APPLIANCE_CMD (0x05)
```
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│ tag_id   │relay_cmd │schedule  │sched_time│duration  │
│  1B      │  1B      │  1B      │  4B      │  2B      │
└──────────┴──────────┴──────────┴──────────┴──────────┘
Total: 9 bytes
```

- `relay_cmd`: 0=off, 1=on, 2=toggle, 3=schedule
- `schedule`: 0=none, 1=on_at, 2=off_at, 3=on_for_duration

## CRC16-CCITT

Polynomial: 0x1021
Initial value: 0xFFFF
No final XOR
Covers: header (9 bytes) + payload (LEN bytes)
Does NOT cover: SOF byte

## Retransmission Policy

- **Critical alerts** (ARC_FAULT, OVERLOAD): Send 3 times, no ACK required
- **Regular data** (CIRCUIT_DATA, APPLIANCE_DATA, SOLAR_DATA): Send once, expect ACK within 5 seconds
- **Commands** (APPLIANCE_CMD, SOLAR_CMD, CALIBRATION): Send once, retry up to 3 times if no ACK
- **Heartbeat**: Send once, no ACK required
- **ACK**: Never retransmitted

## Timing

| Message | Interval | Priority |
|---------|----------|----------|
| HEARTBEAT | 60s | Low |
| CIRCUIT_DATA | 500ms | Normal |
| APPLIANCE_DATA | 5s | Normal |
| SOLAR_DATA | 10s | Normal |
| ARC_FAULT_ALERT | Immediate, 3x | Critical |
| OVERLOAD_ALERT | Immediate, 3x | Critical |
| TIME_SYNC | 3600s (hourly) | Low |