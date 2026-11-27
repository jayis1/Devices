# Local API

`GET /health` reports service availability. `GET /reports` returns local recent reports ordered newest first. `POST /events` accepts protocol-v1 event JSON for local integration tests; deployment must restrict it to the trusted LAN and enforce enrolled IDs. No authentication is implemented in this prototype API; do not expose it to the internet. Errors return HTTP 4xx and do not create records. MQTT ingestion must apply the same validation. Production requirements: authenticated users, per-device authorization, rate limiting, TLS, audit-safe logs, and bounded retention.
