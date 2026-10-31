# CacheReady protocol v1 (reference)

All multibyte integers are little-endian. Frame: version u8 (1), node_id u8, type u8 (1=telemetry), sequence u8, timestamp_seconds u32, payload 0..32 bytes, CRC-16/IBM u16 over all preceding bytes. CRC detects accidental corruption and is not authentication. Receiver rejects unsupported version, oversize/truncated frames, invalid CRC, duplicate sequence, and stale timestamp. Production transport must authenticate/encrypt and bind node identity to per-device credentials.

Payload schema is application-specific pending sensor calibration; reserve explicit type/length fields before hardware interoperability testing. Current portable C framing implementation tests the envelope only.
