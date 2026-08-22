# WasteSort Mesh Protocol Specification

## Transport
- PHY: 868 MHz GFSK / LoRa-compatible narrowband depending node profile
- MAC: Hub-scheduled TDMA
- Addressing: 16-bit source/destination, `0xFFFF` broadcast
- Security: AES-128 CTR on payload, CRC-16 on full frame

## Frame layout

| Field | Bytes | Notes |
|-------|-------|-------|
| Preamble | 4 | `0xAA 0x55 0xAA 0x55` |
| Sync | 2 | `0x37 0xC9` |
| Length | 1 | header + payload |
| Source ID | 2 | little-endian |
| Destination ID | 2 | little-endian |
| Message type | 1 | see below |
| Sequence | 2 | wraps at 65535 |
| Session nonce | 4 | anti-replay |
| Payload | 0-48 | typed |
| CRC16 | 2 | CCITT |

## Message types
- `0x01` JOIN_REQ
- `0x02` JOIN_ACK
- `0x03` HEARTBEAT
- `0x04` SORT_EVENT
- `0x05` BIN_TELEMETRY
- `0x06` PICKUP_EVENT
- `0x07` COMMAND
- `0x08` CONFIG
- `0x09` OTA_CHUNK
- `0x0A` OTA_STATUS
- `0x0B` ALERT

## Sort event payload

| Field | Type |
|-------|------|
| item_id | u32 |
| recommended_stream | u8 |
| material_class | u8 |
| confidence_q15 | u16 |
| contamination_risk_pct | u8 |
| barcode_present | u8 |
| mass_grams | u16 |

## Bin telemetry payload

| Field | Type |
|-------|------|
| stream | u8 |
| fill_pct | u8 |
| mass_grams | u16 |
| voc_index | u16 |
| temp_c_x100 | i16 |
| rh_pct_x100 | u16 |
| lid_open_count | u16 |
| battery_mv | u16 |

## Pickup payload

| Field | Type |
|-------|------|
| event_type | u8 |
| tilt_mdps | i16 |
| lift_peak_mg | u16 |
| solar_mv | u16 |
| battery_mv | u16 |
| next_pickup_epoch | u32 |

## Stream enum
- `0` unknown
- `1` recycle
- `2` compost
- `3` landfill
- `4` glass
- `5` deposit
- `6` special_dropoff
