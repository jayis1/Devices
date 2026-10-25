# Protocol v1

Documented by jayis1.

Transport is intended as a BLE GATT notification: one 20-byte frame per event. Little-endian fields. `firmware/common/protocol.h` is normative.

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 1 | version (1) |
| 1 | 1 | kind: 1 light lux integer, 2 contact 0/1, 3 approach -1 unknown/0 clear/1 blocked, 4 heartbeat |
| 2 | 2 | node_id, unsigned little-endian |
| 4 | 4 | sequence, unsigned little-endian |
| 8 | 4 | age_s, unsigned little-endian |
| 12 | 4 | signed int32 value, little-endian |
| 16 | 2 | CRC-16/CCITT-FALSE over bytes 0–15, init 0xffff, xorout 0 |
| 18 | 2 | reserved, must be zero |

CRC detects accidental corruption only and does not authenticate. Deployment requires BLE authenticated bonding, per-device commissioning and persistent anti-replay sequence handling. The current firmware does not implement radio, discovery, persistence, secure counters, or wrap-safe replay protection. Unknown kinds, bad sizes, CRC errors, wrong version, and nonzero reserved bytes are rejected by the codec. Staleness is evaluated separately from frame acceptance; receiver should label readings stale, not clear.
