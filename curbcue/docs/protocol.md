# Protocol v1

Prototype application event fields: version u8=1; node_id 1–32 printable ASCII chars; seq uint32; kind enum obstruction/wet/tilt/manual/ack; sample_ms uint32 monotonic; severity integer 0–3; optional sensor map with bounded numeric values. Transport is BLE GATT; maximum serialized event 160 bytes. Hub receipt time is authoritative for server ordering. Deduplicate by node_id+seq; reject malformed/out-of-range fields, unknown versions, and events older than configured site retention. Sequence wrap uses modulo 2^32 and must be handled with a bounded recent-event window.

This is a prototype schema, not a secure wire encoding. Production MUST use authenticated BLE pairing, per-device keys, revocation, TLS MQTT, per-site ACLs, topic allow-lists, payload bounds and command authorization. The prototype has no remotely executable commands. Never enable public broker access or infer safety from missing messages.
