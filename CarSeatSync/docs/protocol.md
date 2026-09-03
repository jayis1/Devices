# CarSeatSync Protocol

## Frame layout
| Field | Bytes | Notes |
|-------|-------|-------|
| sync | 1 | 0xA5 |
| version | 1 | protocol version |
| source_id | 1 | node identity |
| msg_type | 1 | seat/child/cabin/handoff/alert |
| trip_id | 4 | little-endian trip session ID |
| payload_len | 1 | 0-48 |
| payload | 0-48 | compact binary payload |
| crc16 | 2 | CCITT over header+payload |

## Source IDs
- `0x01` vehicle hub
- `0x02` safelatch clip
- `0x03` cabin sentinel
- `0x04` child band
- `0x05` handoff beacon

## Security direction
Reference code ships with CRC integrity only. Production builds should add BLE LE Secure Connections, per-node keys, and signed alert frames.
