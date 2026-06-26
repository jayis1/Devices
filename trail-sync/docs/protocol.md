# TrailSync — Communication Protocol Specification

## Overview

TrailSync uses two radio protocols for different range/power requirements:
- **Sub-GHz Mesh (868/915 MHz):** Short-range (50-100m), low-power, coin-cell compatible
- **LoRa Mesh (868/915 MHz):** Long-range (5-15km), medium-power, solar-powered beacons

All frames use CRC16-CCITT for integrity. Binary protocol for efficiency.

## Sub-GHz Mesh Protocol

### Frame Format

```
┌──────────┬──────────┬──────────┬──────────┬─────────┬──────────┐
│ Preamble  │ Sync     │ Header   │ Payload  │ Padding │ CRC16    │
│ 4 bytes   │ 2 bytes  │ 4 bytes  │ N bytes  │ 0-3     │ 2 bytes  │
└──────────┴──────────┴──────────┴──────────┴─────────┴──────────┘
```

**Header (4 bytes):**
| Byte | Field | Description |
|------|-------|-------------|
| 0 | Message Type | ts_msg_type_t (0x10-0xF0) |
| 1 | Source Node ID | Node address (hub=0x00, wrist=0x01-0x0F, pod=0x10-0x2F, beacon=0x40-0xFF) |
| 2 | Sequence | Incrementing sequence number |
| 3 | Flags | Alert flags bitmask |

**CRC:** CRC16-CCITT over header + payload (poly 0x1021, init 0xFFFF)

### Message Types

| Type | ID | Source → Dest | Max Payload | Frequency |
|------|----|---------------|-------------|-----------|
| GAIT | 0x10 | Shoe Pod → Wrist | 24 bytes | Every 5s during run |
| TELEMETRY | 0x20 | Wrist → Hub | 28 bytes | Every 60s |
| NAV | 0x30 | Beacon → Wrist | 22 bytes | Every 60s |
| SOS | 0x40 | Wrist → Hub/Beacon | 20 bytes | On demand |
| SOS_ACK | 0x41 | Hub → Wrist | 8 bytes | On SOS received |
| BEACON_DATA | 0x50 | Beacon → Mesh | 22 bytes | Every 5 min |
| TRAIL_COND | 0x60 | Cloud → Beacon | 10 bytes | On update |
| INJURY_ALERT | 0x70 | Wrist → Hub | 13 bytes | On threshold |
| STORM_ALERT | 0x80 | Wrist/Hub → Mesh | 11 bytes | On prediction |
| HEARTBEAT | 0xF0 | Any → Any | 8 bytes | Every 5 min |

### Alert Flags

| Bit | Flag | Description |
|-----|------|-------------|
| 0 | FALL_DETECTED | Acceleration spike > 8G + stillness |
| 1 | ALTITUDE_SICK | SpO2 < 94% + HRV drop + fast ascent |
| 2 | STORM_INCOMING | Pressure drop > 4 hPa/3hr |
| 3 | OFF_TRAIL | GPS position outside trail corridor |
| 4 | LOW_BATT | Battery < 15% |
| 5 | INJURY_RISK | Gait asymmetry or impact threshold |
| 6 | OVERTRAINING | HRV < 60% of baseline |
| 7 | BEACON_NEAR | Approaching a trail beacon |

## LoRa Mesh Protocol

### Frame Format

LoRa uses the same binary protocol as Sub-GHz but with additional relay headers for mesh forwarding:

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ Preamble  │ Sync     │ Relay    │ Header   │ Payload  │ CRC16    │
│ 8 bytes   │ 2 bytes  │ 4 bytes  │ 4 bytes  │ N bytes  │ 2 bytes  │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
```

**Relay Header (4 bytes):**
| Byte | Field | Description |
|------|-------|-------------|
| 0 | Source Node ID | Original sender |
| 1 | Hop Count | Number of relay hops (0 = direct) |
| 2 | Max Hops | Maximum relay hops (default: 10) |
| 3 | TTL | Time-to-live in seconds (255 = 30 min) |

### LoRa Parameters

| Parameter | Value |
|-----------|-------|
| Frequency | 868 MHz (EU) / 915 MHz (US) |
| Spreading Factor | SF7 (normal) / SF12 (SOS) |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| TX Power | +20 dBm |
| Range | 5 km (SF7) / 15 km (SF12) |
| Airtime | ~100ms (SF7) / ~2s (SF12) |

### SOS Relay

When a Wrist Unit triggers SOS:
1. Wrist broadcasts SOS on both Sub-GHz and LoRa
2. Nearest Trail Beacon receives SOS on LoRa
3. Beacon increments hop count and rebroadcasts on LoRa
4. Each beacon within range rebroadcasts (max 10 hops)
5. Hub receives SOS and relays to cloud via WiFi
6. Hub sends SOS_ACK back through LoRa mesh (directed toward source)
7. Wrist Unit receives ACK and displays "SOS received"

## Pairing Protocol

### Wrist Unit ↔ Shoe Pod (BLE)

1. Wrist Unit enters pairing mode (button hold)
2. Wrist advertises BLE with TrailSync service UUID
3. Shoe Pod enters pairing mode (button press at power-up)
4. Wrist discovers Pod via BLE scan
5. Bonding: exchange encryption keys (AES-128)
6. Wrist sends Sub-GHz mesh parameters to Pod (channel, net ID)
7. Pod acknowledges with its node ID
8. Subsequent communication moves to Sub-GHz mesh (lower power)

### Wrist Unit ↔ Trail Beacon (Sub-GHz)

No pairing needed. Beacons broadcast NAV messages every 60s. Wrist Units receive and cache beacon data.

### Hub ↔ All Nodes

Hub coordinates mesh network assignment. New nodes send HEARTBEAT on joining; Hub responds with node ID assignment and mesh parameters.