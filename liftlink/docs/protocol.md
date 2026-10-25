# Prototype event contract

BLE GATT star; little-endian integer fields. No mesh behavior is implemented.

| Field | Type | Range/meaning |
|---|---|---|
| version | uint8 | exactly 1 |
| node_id | uint16 | provisioned site-local ID |
| sequence | uint32 | increments per node; wrap uses modulo comparison |
| type | uint8 | 1 heartbeat, 2 lift sample, 3 user report, 4 staff acknowledgement |
| uptime_ms | uint32 | monotonic device uptime; never treated as UTC |
| status | uint8 | 0 unknown, 1 likely available, 2 likely unavailable, 3 staff unavailable, 4 staff available |
| confidence | uint8 | 0..100; ignored for staff reports |

Host test codec uses a fixed 16-byte frame (version 1, node ID 2, sequence 4, type 1, uptime 4, status 1, confidence 1, CRC 2) and CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff) over first 14 bytes, stored little-endian. Decoder rejects unsupported versions, invalid type/status/confidence, wrong length and CRC. BLE security is not provided by the test codec. Production must use authenticated BLE pairing and TLS to cloud. MQTT topics: `liftlink/<site>/events` publish; `liftlink/<site>/status` read-only retained status. No retained commands or actuation topic.
