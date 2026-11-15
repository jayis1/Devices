# Protocol

JSON envelope: v=1, node=random provisioned ID, seq=monotonic per node, kind, ts_ms UTC epoch milliseconds, value numeric. Kinds: rack_mass_g, return_passage, heartbeat. Maximum encoded payload 384 bytes. Reject unsupported versions, duplicate sequence, non-finite values and events older than 24h. Current C validation checks version, sequence and finite value only. Production must add bounded JSON parsing, enrolled identity, BLE LE Secure Connections, persisted sequence, clock strategy, replay protection and authenticated commissioning; those are not implemented here.
