# API and MQTT contract

Prototype API base `/api/v1`; JSON timestamps are gateway/server UTC ISO-8601. No authentication is implemented; bind to localhost for development only.

- `GET /health`: process status.
- `POST /events`: accepts `{node_id: 0..65535, sequence: 0..65535, kind: 'fixture'|'path_check'|'health', value_a: integer, value_b: integer, received_at: RFC3339}`. Duplicate `(node_id,sequence)` returns `duplicate: true`.
- `GET /events?limit=100`: newest events, limit 1..500.

MQTT integration topic convention: `refillready/site/{site_id}/node/{node_id}/telemetry`; production must use TLS, client certificates, per-node ACLs, payload caps, and non-retained telemetry. This prototype does not connect a broker automatically.
