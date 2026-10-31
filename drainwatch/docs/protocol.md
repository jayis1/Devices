# Binary telemetry v1

Fixed 16-byte little-endian frame:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | version = 1 |
| 1 | 1 | node_kind: 1 inlet, 2 rain |
| 2 | 4 | node_id unsigned |
| 6 | 4 | sequence unsigned, increments per node |
| 10 | 4 | value signed; inlet distance mm, rain pulse count for interval |
| 14 | 2 | CRC-16/CCITT-FALSE over bytes 0..13 (poly 0x1021, init 0xffff, xorout 0) |

Receiver rejects unknown version/kind, wrong length, CRC failure and duplicate sequence. Sequence wrap uses unsigned modulo arithmetic; receiver must expire replay state after node re-provisioning. No timestamps are in the RF frame; gateway receive time is explicitly gateway time and may be unsynchronized. Units are fixed by kind. CRC is not authentication. Do not deploy where malicious injection matters until per-device authentication and key provisioning are implemented. Radio PHY, addressing and acknowledgements are hardware-adapter responsibilities and are not implemented. No command frames exist.

JSON API mirror: `node_id`, `sequence`, `kind`, `value`, `received_at` (RFC3339 UTC), optional `valid`. Validate bounded integer values and never treat missing data as a normal reading.
