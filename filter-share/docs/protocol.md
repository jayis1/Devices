# FilterShare wire protocol v1

All multibyte integers are little-endian. Frame length is exactly 26 bytes; reject any other length, wrong magic (`0x4653`), unsupported version, unknown type, or CRC mismatch. CRC-16/CCITT-FALSE uses polynomial 0x1021, initial 0xffff, no reflection/xorout, over bytes 0..23; stored in bytes 24..25.

Offsets: 0 magic u16; 2 version u8 (=1); 3 type u8 (1=PM sample, 2=dock state, 3=heartbeat); 4 node_id u32; 8 seq u32; 12 uptime_ms u32; 16 value_a i32; 20 value_b i32; 24 crc u16.

PM sample: value_a = PM2.5 ×10 µg/m³; value_b = PM10 ×10 µg/m³. Dock state: each bit in value_a means corresponding loaner present; value_b marks staff-declared service due. Heartbeat values are zero. Nodes never send personal or borrower identifiers. Gateway receipt time is authoritative; uptime is monotonic modulo 2^32. Reject duplicate sequence values per node/boot; on reboot, begin a fresh session and mark data stale until observed. Transport authentication is provisioned per peer; CRC detects corruption, not forgery. Prototype does not implement replay-resistant key rotation or secure boot.
