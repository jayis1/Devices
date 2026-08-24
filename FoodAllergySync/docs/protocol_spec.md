# FoodAllergySync Radio Protocol

## Physical layer
- Band: 868 MHz ISM
- Modem: LoRa-compatible settings on SX1262 / STM32WL
- Topology: star-of-peers under hub coordinator with TDMA slots
- Uplink interval: 60 s nominal, 5 s during alert windows

## Frame layout
| Offset | Field | Size |
|--------|-------|------|
| 0 | Preamble | 1 |
| 1 | Version | 1 |
| 2 | Message type | 1 |
| 3 | Node type | 1 |
| 4 | Source ID | 2 |
| 6 | Destination ID | 2 |
| 8 | Flags | 1 |
| 9 | Sequence | 1 |
| 10 | Payload length | 1 |
| 11 | Payload | 0-48 |
| 59 | CRC16 | 2 |

## Message types
- `0x01` telemetry
- `0x02` event
- `0x03` alert
- `0x04` command
- `0x05` ack
- `0x06` config
- `0x07` ota_chunk

## Priority alerts
- unsafe ingredient
- positive strip
- lunchbox temperature threshold
- injector missing
- SOS pressed

## Security
- AES-128 CTR payload encryption
- Nonce = home_id + node_id + boot_counter + frame_sequence
- Per-home key rotation supported by hub
