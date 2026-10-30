# API prototype

`GET /health`, `POST /telemetry`, `GET /nodes`. Telemetry JSON matches docs/protocol.md. MQTT topic convention (not connected): `bookdropwatch/v1/{site_id}/{node_id}/telemetry`. API has no authentication, persistent storage, rate limiting, or MQTT client; bind only to loopback during development.
