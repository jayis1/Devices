# Prototype API

GET /health -> {status: "ok"}
POST /telemetry accepts site_id, node_type, node_id, kind, value. Kinds: route, equipment, climate, fault, heartbeat. Returns accepted/site_id/kind. Input size, authentication, persistence, rate limiting, TLS termination, and MQTT ingestion are deployment work; this stub is not internet-ready.
