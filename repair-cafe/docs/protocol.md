# Event contract

Logical event fields: protocol version u8, event type u8, node ID u8, state u8, sequence u32, ticket ID u16, CRC-16/CCITT-FALSE u16, little-endian serialization. CRC covers preceding serialized fields; max encoded length 13 bytes (configured payload ceiling 48). Version is 1. Event types 1=ticket created, 2=bench state, 3=ticket workflow state. States 0=unknown, 1=ready, 2=busy, 3=paused. Sequence is per-node monotonic and must persist through restart in a real implementation. Ticket IDs are random and event-scoped; never encode names or contact details.

Gateway validation: reject bad CRC/version/enum/length, zero IDs or sequence, duplicate and older sequence; preserve receive time and source freshness. No actual BLE wire encoding, acknowledgement, encryption, clock synchronization, or replay-resistant enrollment is implemented in this reference. BLE loss marks bench state stale/unknown. Retry design must be bounded and idempotent, not toggle-based.
