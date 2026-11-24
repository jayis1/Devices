# Data and failure protocol

Prototype event schema, bounds, enum values, sequence deduplication and MQTT topic are in the top-level README. Unknown UTC time is zero; gateway receipt timestamp is authoritative. BLE transport currently has no authentication or encryption and must not be treated as secure. Reject unknown versions and stale/duplicate events; a disconnected node yields unknown state. There are no actuator commands. TLS, enrollment and key rotation are deployment work, not implemented.
