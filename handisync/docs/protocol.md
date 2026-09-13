# Fixed-node protocol

All integers are little-endian. Frame wire form is `version:u8 | type:u8 | node:u32 | seq:u32 | epoch:u32 | len:u8 | payload:0..80 | crc32c:u32 | tag:16`. `tag` is HMAC-SHA256(frame before tag) truncated to 16 bytes; beacons use a commissioning key only during provisioning. Types: `0x01 HEARTBEAT`, `0x02 TELEMETRY`, `0x10 COMMAND`, `0x11 ACK`, `0x12 FAULT`, `0x20 LEASE`.

Commands include `command_id:u16`, `target:u8`, `action:u8`, `duration_s:u16`, `nonce:u32`. Nodes retain the last 32 sequences and command IDs; duplicate commands ACK their prior result without moving hardware. A node accepts commands only in its scheduled downlink slot and only with a lease epoch not older than 30 s.
