# Prototype API

- `GET /health` — service status.
- `POST /events` — JSON telemetry event `{node_id, sequence, kind, value, received_at, valid}`.
- `GET /nodes` — latest event per node.

SQLite local store deduplicates `(node_id, sequence)`. `kind` is `inlet` or `rain`; value is integer millimeters or interval pulse count respectively. Current API is an unauthenticated localhost prototype and is not suitable for a shared network. MQTT integration is not implemented; planned topic is `drainwatch/v1/{site}/{node}/telemetry`, with broker TLS, per-device credentials and ACLs required before deployment. No remote control endpoints.
