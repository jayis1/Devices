# Wire protocol v1

All multibyte fields are little-endian. GATT payload is exactly 16 bytes: `version:u8, type:u8, node_id:u16, sequence:u32, value:i32, flags:u16, crc16:u16`. CRC is CRC-16/CCITT-FALSE (poly 0x1021, init 0xffff, xorout 0), calculated over bytes 0..13. Message types: 1=weight_mg, 2=asset_event (value is configured asset ID; no NFC UID), 3=heartbeat. Unknown versions/types and malformed lengths are rejected. Sequence numbers increment per node; hub drops duplicate or older sequence values using modular 32-bit comparison.

CRC is not authentication. Prototype relies on BLE bonded links only; do not expose this protocol over public or untrusted networks. Future authenticated envelope and key lifecycle are required before production.
