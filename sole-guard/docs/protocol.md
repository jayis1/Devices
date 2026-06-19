# SoleGuard Mesh Protocol Specification

## Transport

Bluetooth SIG Mesh on 2.4 GHz (nRF52840). The ankle tag is a relay (mesh hop) to extend range between insoles and the bedside hub.

## Vendor Model

- Vendor ID: `0x05F0` (placeholder)
- Model IDs: `0x0001` (pressure), `0x0002` (alert), `0x0003` (risk)

## Message Types

| Type | Code | Sender | Struct |
|------|------|--------|--------|
| PRESSURE_TEMP | 0x10 | Insole | 24× pressure + 8× temp + PTI + flags |
| GAIT | 0x11 | Insole/Ankle | 8× gait features (fixed-point) |
| EDEMA | 0x12 | Ankle | impedance + edema_index + skin_temp |
| ALERT | 0x20 | Any | flags + value (mesh-flood, priority) |
| RISK_SCORE | 0x30 | Hub | risk_left + risk_right (broadcast) |
| SCAN_RESULT | 0x40 | Scanner | wound_class + confidence + image_ref + weight |
| HEARTBEAT | 0xF0 | Any | battery_pct |

## Payload Encoding

All payloads are packed structs (little-endian, `__attribute__((packed))`). Each ends with a CRC-16/CCITT (poly 0x1021, init 0xFFFF). See `firmware/common/sole_protocol.h` for exact struct definitions.

## Pressure Scaling

`pressure[i]` is an 8-bit value 0–255, linearly scaled to 0–500 kPa:
`kPa = pressure[i] * 500 / 255`

## Temperature Encoding

`temp_centic[i]` is a signed 16-bit int in centi-degrees Celsius:
`degC = temp_centic[i] / 100.0`

Range: ±327.67°C, resolution ±0.01°C.

## Gait Fixed-Point

| Index | Field | Scale | Units |
|-------|-------|-------|-------|
| 0 | cadence | ×10 | steps/min |
| 1 | stride length | ×1 | mm |
| 2 | symmetry index | ×1000 | 0–1 (1=perfect) |
| 3 | double support | ×100 | % |
| 4 | shuffling score | ×1000 | 0–1 |
| 5 | foot clearance | ×1 | mm |
| 6 | step count | ×1 | count |
| 7 | activity class | ×1 | enum |

## Alert Flags

| Bit | Flag | Meaning |
|-----|------|---------|
| 0x01 | HOTSPOT | Sustained pressure hotspot |
| 0x02 | TEMP_ASYM | Temperature asymmetry > 2.2°C |
| 0x04 | FALL | Fall detected |
| 0x08 | WOUND | Wound detected by scanner |
| 0x10 | LOW_BATT | Low battery |
| 0x20 | EDEMA | Edema index elevated |

## Reporting Cadence

| Message | Period |
|---------|--------|
| PRESSURE_TEMP | 30s |
| GAIT | 30s (insole), 60s (ankle) |
| EDEMA | 4h |
| ALERT | event-driven (mesh-flood, <1s latency) |
| RISK_SCORE | 5min (hub broadcast) |
| HEARTBEAT | 60s |