# API reference (demo)

`GET /health` -> `{"status":"ok"}`. `POST /v1/telemetry` accepts `site_id`, `node_id` (0–255), `sequence` (0–65535), `received_at` (ISO-8601), `occupied_mask`, `valid_mask` (0–65535; occupied must be subset of valid), and `kind` (1 or 2). Duplicate `(site_id,node_id,sequence)` returns HTTP 409. `GET /v1/sites/{site_id}/status` returns aggregate count and number of known seats; no individual record is exposed. In-memory reference only, no authentication or persistence; bind to localhost during development.

MQTT production convention: `libraryloop/v1/{site_id}/{node_id}/telemetry`, QoS 1, TLS; payload uses the fields above. No retained commands or actuation endpoint.
