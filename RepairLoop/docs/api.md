# RepairLoop API

Local FastAPI service; routes in `software/dashboard/app.py`:

- `GET /health`: liveness check.
- `POST /v1/readings`: accepts version, node_type (1–4), node_id and sequence (uint16 values), metric (1–32 chars), numeric value and unit (max 12 chars).
- `GET /v1/nodes`: current in-memory latest values and stale flag (120 seconds).

Prototype intentionally has no authentication, persistence, or MQTT bridge and must not be exposed to an untrusted network. Before deployment add user/device authorization, TLS, bounded storage, durable deduplication, MQTT client ACLs and deletion controls. API has no control endpoints.
