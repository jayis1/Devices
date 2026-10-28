# Protocol v1

MQTT topic: `glowroute/v1/telemetry/{node}`. JSON fields: v=1, node (1..255), seq (uint32), lux (0..200000 lx), uptime_s (uint32). Reject unknown versions, invalid numeric ranges, and stale/duplicate sequence numbers in production. Units are lux and seconds. No commands are defined.

Binary host reference frame is 10 bytes: version u8, node u8, seq u32 big-endian, lux_milli u16 big-endian, CRC16-CCITT u16 big-endian over first 8 bytes. This compact reference currently caps lux at 65.535 lx; use JSON for normal reporting. CRC detects accidental corruption only, not forgery.
