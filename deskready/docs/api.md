# DeskReady interfaces

Local HTTP API: `GET /health`; `POST /events` with JSON v1 (`node`, monotonically increasing `seq`, `kind`, `ready`, `uptime_s`); `GET /stations` returns last accepted event by node. Duplicate/stale sequence returns 409, invalid schema returns 422. Prototype has no authentication, durable storage, MQTT broker, or network transport adapter. Bind to loopback or a trusted isolated LAN only; never expose publicly. Production requires per-node identity/authentication and authorized transport.
