# AirShare protocol

Portable little-endian frame, version 1, fixed 20 bytes: byte 0 version, byte 1 type (1 telemetry), bytes 2-5 node_id uint32, bytes 6-9 sequence uint32, bytes 10-13 uptime_ms uint32, bytes 14-15 CO2 ppm uint16 (0xFFFF unavailable), bytes 16-17 RH centi-percent uint16 (0xFFFF unavailable), bytes 18-19 CRC-16/CCITT-FALSE over bytes 0..17, initial 0xFFFF, polynomial 0x1021. Temperature is carried in the JSON bridge contract rather than this compact frame. CRC detects accidental corruption only, not forgery. Production BLE provisioning/encryption is required.

Reject unsupported versions, types, bad lengths, and bad CRC. Sequence is modulo 2^32; consumer deduplicates within a bounded session window. Timestamp is assigned by hub and marked approximate; no synchronized clock guarantee. No commands or actuation. Radio disconnect resumes latest-value advertisements; historical buffering is not implemented.
