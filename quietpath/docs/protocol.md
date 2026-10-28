# Protocol v1

All integers little-endian. Fixed frame length 19 bytes:

| Offset | Width | Field |
|---:|---:|---|
| 0 | 1 | version = 1 |
| 1 | 1 | kind (1 telemetry) |
| 2 | 2 | node_id |
| 4 | 2 | sequence |
| 6 | 4 | epoch_minutes |
| 10 | 2 | Leq centi-dB, `0xffff` invalid |
| 12 | 2 | peak centi-dB, `0xffff` invalid |
| 14 | 1 | flags bit0 sensor_fault, bit1 clock_uncertain |
| 15 | 2 | reserved zero |
| 17 | 2 | CRC-16/CCITT-FALSE over bytes 0–16 |

CRC polynomial 0x1021, init 0xffff, xorout 0; no reflection. Reject unsupported versions, wrong length, nonzero reserved, unknown flags, invalid CRC, values outside 0–14000 centi-dB. Receiver deduplicates node/sequence and validates a 20-minute timestamp window. Sequence wrap is modulo 65536; production receiver must expire dedup state and handle device reboot explicitly. CRC provides no authenticity. Production radio must be provisioned with unique authenticated encryption keys; do not use unencrypted LoRa in public deployment. There are no downlink commands.
