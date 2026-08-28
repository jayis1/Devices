# RoutineSync Protocol Specification

## 1. Physical links
- BLE 5.3 GATT for telemetry, control, provisioning
- UWB (IEEE 802.15.4z HRP) for ranging bursts

## 2. Application frame

| Field | Bytes | Description |
|-------|-------|-------------|
| preamble | 4 | 0xAA55AA55 |
| sync | 2 | 0x52 0x53 |
| length | 1 | payload length + header bytes |
| src_id | 2 | source node |
| dst_id | 2 | destination node |
| msg_type | 1 | message opcode |
| seq | 2 | monotonically increasing per sender |
| session_nonce | 4 | anti-replay session value |
| payload | 0-48 | typed payload |
| crc16 | 2 | CRC-16/CCITT |

## 3. Message types
- `0x01` JOIN_REQ
- `0x02` JOIN_ACK
- `0x03` HEARTBEAT
- `0x04` ITEM_TELEMETRY
- `0x05` DOORWAY_STATUS
- `0x06` FOCUS_STATE
- `0x07` FIND_REQUEST
- `0x08` FIND_RESPONSE
- `0x09` COMMAND
- `0x0A` CONFIG
- `0x0B` ALERT

## 4. Security
- household-scoped AES-128-CTR payload encryption
- rotating session nonce on each join
- QR-provisioned bootstrap secret
- replay rejection via `(src_id, seq, nonce)` cache

## 5. Timing
- tag heartbeat: 2 s idle, 200 ms locate mode
- doorway poll: 5 Hz during active departure
- focus beacon summary: every 10 s active, 60 s idle
- UWB ranging burst: 3 rounds over 600 ms window
