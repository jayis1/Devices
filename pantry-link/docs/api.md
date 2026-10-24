# API reference (demo)

Base URL defaults to `http://127.0.0.1:8000`; bind to loopback only. The sample has no user authentication, tenant authorization, persistent database, or rate limiting. Do not expose it to a network.

- `GET /health` → `{ "ok": true }`
- `POST /v1/telemetry` accepts `{site_id,node_id,sequence,kind,value,unit,timestamp}`. Demo limits: site/node 1–64 chars, sequence nonnegative integer, kind temperature/mass/door/heartbeat, finite numeric value, timestamp UTC ISO string. Duplicate (site,node,sequence) returns 409.
- `GET /v1/sites/{site_id}/inventory` gives in-memory adjustment rows for the process lifetime; this is not a stock ledger.

Production requirements: authenticate users and devices, enforce site-level object authorization, MQTT TLS and per-device ACLs, bounded payloads/timeouts, durable idempotent storage, retention/deletion, audit trail, and credential rotation.
