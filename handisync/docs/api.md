# API

`GET /health` returns service state. `GET /v1/nodes` returns latest redacted node state. `POST /v1/commands` accepts `{node_id, action, duration_s, confirmation}` and returns `202` after policy validation; the hub remains authoritative. `POST /v1/events` accepts signed hub events. Send `Authorization: Bearer <HANDISYNC_API_TOKEN>`. The baseline service intentionally exposes no unauthenticated actuator endpoint.
