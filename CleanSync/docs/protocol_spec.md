# CleanSync Protocol Specification

## PHY / MAC
- Band: 863-870 MHz (EU profile shown; retune for local regulation)
- Modulation: LoRa-style long-range mode for join, GFSK TDMA for routine telemetry
- Channel plan: 3 primary telemetry channels + 1 maintenance channel
- Hub owns the superframe and allocates slots.

## Frame Format

| Field | Bytes | Notes |
|------|-------|-------|
| preamble | 2 | `0xA5 0x5A` |
| version | 1 | protocol version |
| src_id | 2 | source node |
| dst_id | 2 | destination node |
| msg_type | 1 | enum |
| seq | 2 | incrementing per sender |
| session_nonce | 4 | rotates each boot |
| payload_len | 1 | 0-48 |
| payload | N | encrypted |
| crc16 | 2 | CCITT |

## Message Types
- `0x01` JOIN_REQ
- `0x02` JOIN_ACK
- `0x10` DIRT_TELEMETRY
- `0x11` DOCK_STATE
- `0x12` WAND_SCAN_SUMMARY
- `0x20` ALERT
- `0x30` COMMAND_PREPARE_MISSION
- `0x31` COMMAND_SANITIZE_DOCK
- `0x32` COMMAND_LED_IDENTIFY
- `0x40` OTA_CHUNK
- `0x41` OTA_STATUS

## Reliability
- Telemetry: best effort with repeat-on-miss next slot
- Alerts: 3 retransmissions + ACK required
- Commands: ACK required within 300 ms or retried up to 5 times

## Commissioning
1. Mobile app pairs over BLE.
2. Node receives household ID, mesh key, node label, room assignment.
3. Node sends JOIN_REQ on maintenance channel.
4. Hub allocates TDMA slot and publishes registration event.
