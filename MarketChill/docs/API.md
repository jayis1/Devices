# API and MQTT contract

Development FastAPI service: `GET /health`, `POST /v1/telemetry`, `GET /v1/telemetry/latest`. Payload: `node_id` (1–32 printable chars), `kind` (`probe` or `door`), `sequence` (0–65535), `received_at` (ISO-8601), optional `temperature_c` (−40..125), optional `door_open`, `battery_mv` (0..6000). The sample app is unauthenticated and in-memory: bind locally only. Production needs user authentication, per-site authorization, TLS, persistence, retention controls, rate/size limits, schema migrations, and audit logging.

Optional broker topic: `marketchill/v1/<site_id>/<node_id>/telemetry`; use QoS 1, no retained messages. Subscriber ACLs should be site-scoped. No command topics are defined. Example credentials must be supplied out-of-band; never commit secrets. MQTT delivery is optional; local alarm behavior does not depend on it.
