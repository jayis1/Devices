# Protocol v1

Little-endian frame, total length 9–57 bytes.

| Offset | Size | Field | Meaning |
|---:|---:|---|---|
| 0 | 1 | version | 1 |
| 1 | 1 | node_type | 1 hub, 2 bench, 3 dock, 4 air |
| 2 | 2 | node_id | unique provisioned ID |
| 4 | 2 | sequence | incrementing modulo 65536 |
| 6 | 1 | payload_len | 0–48 |
| 7 | variable | payload | application-defined TLV; units specified by each measurement |
| 7+length | 2 | CRC | CRC-16/CCITT-FALSE, init FFFF, poly 1021, no reflection, xorout 0000 |

Reject wrong version, inconsistent length, payload over 48, and bad CRC. Consumer deduplicates by node ID and sequence; mark observations stale at 120 s and do not execute commands from this telemetry protocol. CRC is not cryptographic authentication. Production must wrap frames in authenticated encryption with unique per-device keys and replay protection; current source is a transport-format demonstration only. BLE connection security and key lifecycle remain implementation requirements.
