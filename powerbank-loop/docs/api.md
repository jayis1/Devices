# API reference (prototype)

`software/dashboard/app.py` exposes:

- `GET /health` → `{"status":"ok"}`.
- `POST /v1/events` accepts JSON event: `v`, `site`, `node`, `seq`, `ts`, `kind`, `payload`. Valid kind is `DOCK_STATUS`, `PACK_STATUS`, or `HEARTBEAT`; kind-specific payload keys/ranges are checked. Body is limited to 4096 bytes. A sequence equal to or older than the last sequence for `(site,node)` returns `{"accepted":false,"reason":"duplicate_or_stale"}`. A newer event returns HTTP 202 and updates in-memory latest state.
- `GET /v1/sites/{site}/latest` returns the latest in-process events for that site.

This prototype has no authentication, persistent database, retention worker, or multi-process consistency. The separate MQTT bridge forwards authenticated broker messages over TLS, but the API itself does not authenticate its caller. Do not expose the API to a network or send real site data until TLS termination, operator/service authentication, per-site authorization, broker ACLs, rate limiting, persistence, retention/deletion, and security review are implemented. API accepts no commands and cannot operate the dock.
