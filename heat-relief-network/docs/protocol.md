# Protocol draft

BLE GATT is the node-to-gateway link; encrypted LE Secure Connections with operator enrollment is required in deployment. This source provides a portable application frame only; it does not implement BLE or cryptography. Fields are serialized explicitly little-endian; never transmit a native struct.

Header: magic u16 `0x4852`, version u8 `1`, type u8, node_id u32, sequence u32, payload_len u16, payload (0..32 bytes), CRC-16/CCITT-FALSE u16 over header and payload. Total length is exactly 14 + payload length. CRC detects accidental corruption, not tampering. Reject unknown versions, truncation, oversized payload, invalid type, bad CRC, and sequence not newer than persisted last sequence. On sequence wrap, re-enroll. BLE provides link security; replay protection and key persistence require platform integration and are not yet implemented.

Types: 1 climate (temperature centi-degrees C signed i16, RH centi-percent u16, CO2 ppm u16, quality u8); 2 supply (water grams u16, crossing count u16, quality u8); 3 health (battery mV u16, status u8). Values require plausibility checks and quality flags. Sampling target 60 s; retry with capped exponential backoff. No remote command messages are defined.
