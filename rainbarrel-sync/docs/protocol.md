# Radio protocol v1

Encoding is little-endian: version u8, type u8, node_id u8, payload_len u8, boot_id u32, sequence u32, uptime_s u32, payload (0–24 bytes), CRC-16/CCITT-FALSE u16 over header and payload. Max frame 42 bytes. Unknown versions, wrong exact length, oversize payloads, or CRC mismatch are rejected.

Types: 1 heartbeat, 2 tank distance (u16 mm, flags u8), 3 rain tip count (u32), 4 demand pulse count (u32). CRC is not a security mechanism. Retries retain the same boot ID and sequence; receiver deduplicates on (node, boot, sequence), bounded to recent frames. Radio framing/retry scheduling remains driver work.
