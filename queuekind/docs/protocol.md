# QueueKind event protocol, version 1

Transport is a BLE GATT characteristic carrying one frame per event. Integers are unsigned little-endian; never transmit native C structs. Layout: magic `0x514B` (2 bytes), version (1, value 1), message type (1: arrival=1, service-complete=2, pause=3, resume=4), node ID (1), sequence (2), payload length (1, maximum 8), payload (0–8), CRC-16/CCITT-FALSE (2, little-endian). CRC covers all preceding bytes: polynomial 0x1021, init 0xFFFF, xorout 0. Max frame size 18 bytes. Current defined events have zero-length payload.

Receiver rejects short/long frames, unknown type/version, incorrect magic, payload length mismatch, CRC mismatch, duplicate sequence and stale event. Sequence wrap comparison is modulo 2^16; nodes must be re-paired/reset by staff if hub state is lost. This does not provide authentication, replay resistance across hub reset, or confidentiality; protocol is demonstrative and not suitable for untrusted radio deployments. Implement cryptographic per-node authentication and durable replay state before production.

Common test vector: empty-payload arrival from node 1, sequence 1, expected pre-CRC bytes `51 4b 01 01 01 01 00`; CRC is computed by the supplied implementation and asserted by host tests. CRC is error detection only.
