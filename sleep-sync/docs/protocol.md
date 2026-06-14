# SleepSync — Protocol Specification

## Overview

SleepSync uses **BLE 5.0 Mesh** for all local communication between hardware nodes. The nightstand hub bridges the mesh to WiFi/cloud via MQTT and provides a BLE GATT server for direct mobile app connection.

## BLE 5 Mesh

### Network Configuration
| Parameter | Value |
|-----------|-------|
| Protocol | BLE 5.0 Mesh (managed flooding) |
| PHY | 1 Mbps (standard) |
| TX Power | 0 dBm (strip), +4 dBm (hub/climate/shade) |
| Range | ~10m indoor (single bedroom) |
| Provisioner | Nightstand Hub (ESP32-S3) |
| Max Nodes | 6 (1 hub + 1 strip + 1 climate + 3 shades) |
| Security | AES-CCM 128-bit (NetKey + AppKey) |
| IV Index | Incremented on security update |

### Provisioning Flow
```
1. Hub boots → starts BLE advertising as unprovisioned device
2. Mobile app scans → discovers "SleepSync-Hub-XXXX"
3. User selects hub → app sends NetKey + AppKey via provisioning protocol
4. Hub becomes provisioner → starts scanning for unprovisioned nodes
5. Sleep strip powers on → advertises as unprovisioned
6. Hub auto-invites strip → provisions with same NetKey + AppKey
7. Repeat for climate node, shade controllers
8. All nodes exchange mesh messages on same network
```

### Message Types (Vendor Model Opcodes)

| Type | Opcode | Direction | Payload Size | Frequency |
|------|--------|-----------|-------------|-----------|
| SLEEP_DATA | 0x01 | Strip → Hub | 11 bytes | Every 5s |
| ENV_DATA | 0x02 | Climate → Hub | 10 bytes | Every 30s |
| SHADE_STATUS | 0x03 | Shade → Hub | 11 bytes | Every 60s |
| HUB_COMMAND | 0x04 | Hub → Any | Variable | On event |
| HUB_SYNC | 0x05 | Hub → All | 8 bytes | Every 60s |
| ALARM_TRIGGER | 0x06 | Hub → All | 4 bytes | On alarm |
| ACK | 0x07 | Any → Hub | 2 bytes | On command receipt |
| OTA_BLOCK | 0x08 | Hub → Any | 32 bytes + header | During OTA |

### Wire Format

All mesh messages use this frame format:

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬────────────┬──────────┐
│ SYNC(2)  │ LEN(1)   │ SRC(1)   │ DST(1)   │ TYPE(1)  │ PAYLOAD  │ CRC16(2)   │          │
│ 0x595E   │ 0-50     │ Node ID  │ Node ID  │ MSG_*    │ 0-50 B   │ CCITT      │          │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴────────────┴──────────┘
```

- **SYNC**: 0x595E (constant)
- **LEN**: Payload length (0-50 bytes)
- **SRC**: Source node ID (0x00-0x06)
- **DST**: Destination node ID (0xFF = broadcast)
- **TYPE**: Message type (MSG_* constant)
- **PAYLOAD**: Message-specific data (see below)
- **CRC16**: CRC-16/CCITT over LEN + SRC + DST + TYPE + PAYLOAD

### Node IDs
| ID | Node |
|----|------|
| 0x00 | Nightstand Hub |
| 0x01 | Sleep Strip |
| 0x02 | Climate Node |
| 0x03 | Shade Controller 1 |
| 0x04 | Shade Controller 2 |
| 0x05 | Shade Controller 3 |
| 0xFF | Broadcast |

## Payload Definitions

### SLEEP_DATA (0x01) — 11 bytes

| Offset | Size | Field | Unit | Range |
|--------|------|-------|------|-------|
| 0 | 2 | heart_rate | BPM × 10 | 300-1200 |
| 2 | 1 | hrv | Unitless | 0-255 |
| 3 | 2 | resp_rate | Breaths/min × 10 | 60-300 |
| 5 | 1 | rrv | Unitless | 0-255 |
| 6 | 1 | movement | Intensity | 0-255 |
| 7 | 1 | snoring | Intensity | 0-255 |
| 8 | 1 | sleep_stage | Enum | 0-3 |
| 9 | 1 | stage_conf | Confidence × 100% | 0-255 |
| 10 | 1 | battery_pct | Percent | 0-100 |

### ENV_DATA (0x02) — 10 bytes

| Offset | Size | Field | Unit | Range |
|--------|------|-------|------|-------|
| 0 | 2 | temperature | °C × 100 | -4000 to 8000 |
| 2 | 2 | humidity | %RH × 100 | 0-10000 |
| 4 | 2 | co2_ppm | ppm | 0-40000 |
| 6 | 1 | hvac_state | Bitfield | Bit0-2 |
| 7 | 1 | heater_state | Enum | 0-2 |
| 8 | 1 | humidifier_state | Enum | 0-2 |
| 9 | 1 | errors | Bitfield | Bit0-2 |

### SHADE_STATUS (0x03) — 11 bytes

| Offset | Size | Field | Unit | Range |
|--------|------|-------|------|-------|
| 0 | 1 | position | Percent | 0-100 |
| 1 | 2 | ambient_light | Lux | 0-120000 |
| 3 | 1 | led_warm | PWM | 0-255 |
| 4 | 1 | led_amber | PWM | 0-255 |
| 5 | 1 | led_cool | PWM | 0-255 |
| 6 | 4 | dawn_time | Unix timestamp | uint32 |
| 10 | 1 | errors | Bitfield | Bit0-2 |

### HUB_COMMAND (0x04) — Variable

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | target_id | Destination node |
| 1 | 1 | cmd_type | CMD_* constant |
| 2 | 1 | param_len | Following params length |
| 3+ | 0-24 | params | Command-specific |

### Command Types

| Cmd | Code | Target | Params |
|-----|------|--------|--------|
| CLIMATE_SETPOINT | 0x01 | Climate | temp_i16(2) + hum_u16(2) |
| CLIMATE_HVAC | 0x02 | Climate | mode_u8(1) |
| SHADE_POSITION | 0x03 | Shade | position_u8(1) |
| SHADE_DAWN | 0x04 | Shade | dawn_time_u32(4) |
| ALARM_SET | 0x05 | Hub | window_start_u32(4) + window_end_u32(4) |
| ALARM_CANCEL | 0x06 | Hub | none |
| SOUND_SET | 0x07 | Hub | sound_id_u8(1) + volume_u8(1) |
| OTA_TRIGGER | 0x08 | Any | target_id(1) + size_u32(4) + crc_u16(2) |

## CRC-16/CCITT

Polynomial: 0x1021, Initial value: 0xFFFF

```c
uint16_t crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}
```

## Timing Requirements

| Node | TX Interval | Max Latency | Priority |
|------|------------|-------------|----------|
| Sleep Strip | 5s | <500ms | Critical |
| Climate Node | 30s | <2s | Medium |
| Shade Controller | 60s | <5s | Low |
| Hub Sync | 60s | <1s | High |
| Hub Command | On event | <500ms | High |
| Alarm | On event | <200ms | Critical |

## Power Budget

| Node | Average Current | Battery | Expected Life |
|------|----------------|---------|---------------|
| Sleep Strip | ~50µA (avg) | 100mAh | ~14 days |
| Nightstand Hub | ~350mA (audio on) | 2000mAh + USB | ~8h (backup) |
| Climate Node | ~80mA | USB powered | N/A |
| Shade Controller | ~200mA (idle), ~1A (motor) | 12V adapter | N/A |