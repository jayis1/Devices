# UroSync Protocol Spec

## Physical layer
- 868 MHz Sub-GHz mesh, star-of-stars under hub supervision
- GFSK or LoRa-compatible framing depending deployment profile
- 19.2 kbps default control profile

## Frame layout
| Field | Bytes | Notes |
|------|-------|-------|
| preamble | 4 | 0xAA55AA55 |
| sync | 2 | 0x52 0xA7 |
| length | 1 | payload-dependent |
| src_id | 2 | sender node |
| dst_id | 2 | receiver node |
| msg_type | 1 | telemetry / config / alert |
| seq | 2 | monotonically increasing |
| session_nonce | 4 | key-stream diversification |
| payload | 0-48 | packed telemetry |
| crc | 2 | CRC-16/CCITT |

## Message families
- `US_MSG_VOID_EVENT`: urine chemistry and uroflow summary
- `US_MSG_MAT_EVENT`: transfer safety summary
- `US_MSG_BOTTLE_EVENT`: hydration adherence summary
- `US_MSG_ENV_EVENT`: bathroom environment / leak summary
- `US_MSG_ALERT`: hub-to-node intervention request

## Security
- AES-128 CTR payload encryption
- rotating household session nonce
- install-time key derivation from QR onboarding secret

## Latency goals
- urgent alert: < 300 ms node to hub
- standard telemetry: < 5 s during active session, < 60 s idle
