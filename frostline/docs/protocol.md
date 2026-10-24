# Protocol v1

Frame is exactly 23 bytes, little-endian multibyte values. CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff, xorout 0) covers bytes 0-20; stored low byte first at 21-22. CRC provides error detection, not authentication.

| Offset | Size | Field | Unit/range |
|---:|---:|---|---|
| 0 | 1 | version | 1 |
| 1 | 1 | message_type | 1 telemetry |
| 2 | 2 | node_id | unsigned |
| 4 | 2 | sequence | wraps modulo 65536 |
| 6 | 4 | unix_time_s | UTC seconds |
| 10 | 2 | surface_temp_c_centi | signed centi-degrees C; -32768 means unavailable |
| 12 | 2 | air_temp_c_centi | signed centi-degrees C; -32768 means unavailable |
| 14 | 2 | rh_permille | 0..1000; 65535 unavailable |
| 16 | 2 | wetness_u16 | calibrated arbitrary count; 65535 unavailable |
| 18 | 2 | battery_mv | millivolts |
| 20 | 1 | flags | bit0 sensor fault, bit1 clock invalid |
| 21 | 2 | crc16 | little-endian |

Receiver rejects wrong length, unsupported version, CRC failure, duplicate sequence (hub implementation pending), and malformed sensor ranges. Radio-level retries and duty cycle are region-configured; nodes jitter reports and cap retries. Until authenticated encryption is implemented, treat all received messages as untrusted and advisory. No commands/OTA are defined in v1.
