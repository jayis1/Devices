# ParkPlay telemetry protocol v1

All integers are little-endian. Advertising payload is exactly 16 bytes; maximum accepted frame is 16 bytes. CRC is CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff, xorout 0).

| Offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 1 | version | `1` |
| 1 | 1 | type | 1 motion count, 2 surface reading, 3 staff event |
| 2 | 4 | node_id | provisioned non-secret identifier |
| 6 | 4 | sequence | monotonically increasing, wraps modulo 2^32 |
| 10 | 4 | value | type-specific signed fixed-point value |
| 14 | 2 | crc | CRC over bytes 0..13 |

Type 1 value: motion count delta, saturating at INT32_MAX. Type 2: temperature in centi-degrees Celsius; wetness electrode is not encoded in v1 and must be reported separately in a future version. Type 3: enumerated staff event 1=requested, 2=in progress, 3=inspection complete (not safety approval). Unknown type/version is rejected. Receiver rejects frames not exactly 16 bytes, CRC failures, and duplicate/out-of-order sequence values; reboot requires re-enrollment or sequence-state recovery. Timestamp is receiver-assigned and marked as such.

CRC detects corruption only. It does not authenticate, encrypt, prevent deliberate replay, or make BLE advertisements private. Production requires authenticated BLE pairing and per-node keys, replay window, and key rotation; these are not implemented by this host example. No actuator commands exist in v1.
