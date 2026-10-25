# Wire protocol v1

All multi-byte integers little-endian. Fixed 16-byte frame: `version:u8, type:u8, node_id:u16, sequence:u16, uptime_s:u32, value_a:i16, value_b:i16, crc16:u16` (CRC-16/CCITT-FALSE over preceding 14 bytes). Units depend on `type`; raw values are never interpreted across types. CRC detects accidental corruption, not hostile modification. BLE pairing in this prototype is not a production identity system.

| Type | Direction | value_a | value_b |
|---|---|---|---|
| 1 fixture | fixture→gateway | flow pulses / 10 s | leak 0/1 |
| 2 path check | check node→gateway | check code 0 unknown, 1 clear, 2 blocked | battery mV |
| 3 health | any→gateway | battery mV | fault bitmask |

Sequence wraps modulo 65536. Receiver rejects unsupported version, invalid CRC, unknown type, and duplicate sequence within its bounded recent window. `uptime_s` is monotonic, not wall-clock. Backend attaches receive time. Commands/actuation are intentionally not part of v1. Implementations must check all ranges before use.
