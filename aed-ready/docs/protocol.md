# AEDReady wire protocol v1

Advertising/application payload is exactly 24 bytes, little-endian. `firmware/common/protocol.h` plus protocol.c are the executable codec source of truth.

| Offset | Width | Field | Meaning |
|---:|---:|---|---|
| 0 | 1 | version | 1 |
| 1 | 1 | kind | 1 = cabinet status |
| 2 | 2 | node_id | provisioned uint16 |
| 4 | 4 | sequence | monotonically increasing uint32; reset requires re-pair/re-baseline |
| 8 | 4 | uptime_s | seconds since boot |
| 12 | 2 | temperature_centi_c | signed °C × 100 |
| 14 | 2 | battery_mv | millivolts |
| 16 | 2 | flags | bit0 door open, bit1 sensor valid, bit2 low battery |
| 18 | 2 | crc16 | CRC-16/CCITT-FALSE over bytes 0–17 |
| 20 | 4 | reserved | must be zero |

Receiver checks exact length, version, kind, reserved bytes, CRC and sequence freshness. Repeated/out-of-order samples are discarded. CRC is not authentication. BLE bonding, authenticated provisioning, anti-replay across reboot, signed OTA and key revocation remain unimplemented. No command or remote actuator message exists. Gateway timestamps reception; there is no synchronized wall clock in the frame. On link loss the state becomes unknown/stale, never normal.