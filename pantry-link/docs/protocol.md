# PantryLink protocol v1

Transport is BLE GATT between enrolled node and hub. Frames use explicit byte serialization, never compiler-packed structs. All multibyte integers are little-endian. CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff, xorout 0) covers header and payload; known check vector `123456789` is `0x29B1`. CRC is error detection, not security: BLE authenticated bonding is required for privacy/authenticity.

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 1 | version (=1) |
| 1 | 1 | kind (1 temperature, 2 mass, 3 door, 4 intake, 5 heartbeat) |
| 2 | 2 | node_id |
| 4 | 2 | boot-scoped sequence |
| 6 | 1 | payload length (0..32) |
| 7 | 1 | flags |
| 8 | 0..32 | payload |
| 8+len | 2 | CRC, little-endian |

Temperature payload: signed int16 centi-degrees C. Mass payload: unsigned uint32 grams. Door payload: one byte (0 closed, 1 open). Intake payload: uint16 item-class code plus uint16 grams. Firmware rejects unknown versions, oversize/truncated frames, bad CRC, duplicate sequence and implausible sensor values before acting. Sequence is scoped by boot; hub records boot changes and rejects replay within a session. There are no remote actuation commands in v1. BLE pairing is physically authorized; production provisioning, key revocation, signed OTA, and cryptographic device identity remain unimplemented.
