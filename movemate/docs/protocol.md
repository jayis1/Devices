# Protocol and operations

Wire frame: version u8 (=1), type u8, payload length u16 big-endian (0..128), sequence u32 big-endian, payload bytes, CRC-16/CCITT-FALSE u16 big-endian. CRC detects accidental corruption only; it is not authentication. Decoder rejects unknown versions, truncation, oversized payloads, trailing bytes, and bad CRC. Sequence deduplication is gateway responsibility. No radio encryption/key lifecycle is implemented in this prototype.

Explicit scan/check-in is the only authoritative box-room assignment. BLE RSSI never moves a box automatically. Offline gateway database is authoritative; cloud sync and MQTT adapter are future work. The intended topic/QoS policy is described in the system README, but no MQTT broker integration is implemented.
