# Shared protocol notes

Logical JSON observation: v=1, node_id (1–64 chars), seq unsigned 32-bit, kind, nullable UTC epoch seconds, values object. Physical firmware reference is a small C frame (`version:u8, kind:u8, sequence:u32, value:u16`) and is not yet a complete codec for that logical schema. Node IDs, authentication, serialization, CRC, replay protection, and BLE GATT definitions remain implementation work. Payloads must be bounded; reject invalid version/kind and stale readings. No command or actuator messages are defined.
