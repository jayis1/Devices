# Protocol and API

Refer to architecture.md for threat model. v1 JSON envelope fields: v, node, seq, ts, kind, value, unit, battery_mv. Mass is grams; temperature is hundredths °C; door is 0/1. Payload max 160 bytes. Gateway deduplicates node/seq. Reject malformed, unknown, out-of-range, or stale telemetry. Never interpret stale data as safe. MQTT QoS 1 telemetry topic `commonshelf/{site}/{node}/telemetry`, no retained payload; availability retained. TLS and site-scoped ACLs required in deployment. Development API intentionally unauthenticated; bind localhost only.
