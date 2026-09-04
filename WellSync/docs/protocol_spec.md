# WellSync Protocol Spec

## Physical links

- 868 MHz FSK/LoRa TDMA for property-wide coverage.
- RS-485 Modbus RTU for mechanical-room wired links.
- BLE 5.0 for sleepy Tap Sentinel provisioning and direct installer diagnostics.

## Frame layout

| Field | Size | Notes |
|------|------|-------|
| preamble | 4 B | `0xAA55AA55` |
| sync | 2 B | `0x57 0xA9` |
| length | 1 B | payload length |
| src_id | 2 B | sender node |
| dst_id | 2 B | destination |
| msg_type | 1 B | WellSync message enum |
| seq | 2 B | monotonic per session |
| session_nonce | 4 B | anti-replay context |
| payload | 0-48 B | typed body |
| crc | 2 B | CRC-16/CCITT |

## Reliability

- all alerts require ACK from hub;
- weather and tap events are retryable low-priority frames;
- pump-disable commands require dual confirmation at hub and controller;
- OTA blocks are chunked and individually hashed.

## Advisory policy

A `do_not_drink` advisory is emitted when any two severe indicators align, or one catastrophic indicator occurs:
- turbidity > 4.0 NTU after storm event;
- ORP collapse below configured treatment threshold;
- dry-run score > 85%;
- UV intensity loss with active faucet consumption during advisory window.
