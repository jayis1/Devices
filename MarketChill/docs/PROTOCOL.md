# MarketChill BLE telemetry v1

Prototype advertising payload is a compact application record; BLE advertising transport is best-effort and unauthenticated. Do not use as a production security boundary.

| Offset | Width | Field | Encoding |
|---:|---:|---|---|
| 0 | 1 | version | 1 |
| 1 | 1 | kind | 1 probe, 2 door |
| 2 | 2 | sequence | unsigned LE, wraps modulo 65536 |
| 4 | 2 | temperature | signed LE centi-degrees C; probe only; door uses 0x8000 sentinel |
| 6 | 1 | door state | 0 closed, 1 open, 0xFF not applicable |
| 7 | 2 | battery | unsigned LE millivolts |

Exactly 9 payload bytes. Reject unknown versions/kinds, wrong length, invalid door state, and probe sentinel. Reception time is assigned by hub; devices do not claim synchronized clocks. Hub suppresses repeated sequence numbers per node and marks records stale after 60 seconds. Sequence wrap is accepted only after a changed value; reboot identity/session handling is not production-ready. BLE link-layer CRC detects transmission errors but application payload is not authenticated or replay-protected. Production requires authenticated provisioning, confidentiality/integrity, rotation/revocation, and security review. No commands or actuators are supported. Regional radio requirements and permitted transmit power must be verified for target markets.
