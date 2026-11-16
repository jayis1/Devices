# Protocol v1

Portable firmware envelope is `rt_event` in `firmware/common/protocol.h`. Kinds: 1 session_start, 2 session_end, 3 book_checkin, 4 reminder_ack. Fields: version (must equal 1), kind, unsigned 16-bit sequence, UTC seconds timestamp, bounded node ID and optional book UUID. Host struct is not a wire encoding: production adapters must define byte order, framing, authentication tag, and payload length explicitly before radio transmission.

Duplicate key is `(node, seq)` within a household; backend demo deduplicates in memory. Sequence wrap requires a provisioned boot/session epoch and persistent dedupe window, not implemented. Reject malformed/unknown version/kind; no default action on invalid input. MQTT topic contract is documented in top-level README. Production security, replay protection, key rotation, reliable acknowledgements, and OTA signing are not implemented.
