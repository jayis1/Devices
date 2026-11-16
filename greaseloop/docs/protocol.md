# Protocol v1 specification

Transport assumption: prototype samples output JSON but do not implement Wi-Fi, BLE, MQTT, cryptographic authentication, or retransmission. JSON UTF-8 object maximum 512 bytes. Fields: v (integer exactly 1), node (1..48 ASCII letters/digits/underscore/hyphen, locally enrolled), seq (unsigned monotonic integer scoped to node), ts (UTC Unix seconds), kind (deposit | collection | container_status | environment), mass_g (signed integer -500..20000; negative values allow tare drift diagnostics only). For environment events this field is reserved and must be zero; a future schema should replace it with explicit temperature/leak fields.

The hub uses (node,seq) as idempotency key; QoS 1 duplicates are discarded. Unknown node, unsupported version, malformed JSON, out-of-bounds values and oversized payloads are rejected. Hub receipt timestamp should be saved by a production implementation alongside node ts. Sequence gaps are visible as missing data, not interpolated. Clock time is advisory and does not authorize actions. There are no remote actuator commands in v1.

CRC is not authentication. Production pairing, replay-resistant credentials, TLS, per-node MQTT ACLs and revocation are mandatory before untrusted-network use. Key rotation and signed firmware update remain unimplemented.
