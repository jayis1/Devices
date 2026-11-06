# Architecture and API

Gateway receives sensor records, validates the fixed frame and forwards telemetry. Line Sentinel load cells indicate coarse occupancy only; Weather Sentinel emits rain-tip count and wind speed. No identity or garment data is collected. Gateway/cloud outage: local observation continues; cloud outage means no remote alerts. Missing/invalid sensors produce unknown, not an all-clear. Operator checks line conditions directly.

API: `POST /v1/telemetry` accepts `{v,node_id,seq,kind,value,unit}`; duplicate or older sequence per node returns 409. `GET /v1/status` returns latest accepted records; `/health` is a liveness endpoint. The reference in-memory store loses state on restart and has no auth; do not expose it publicly. Production requires TLS, authenticated MQTT ingestion, authorization, persistence, rate limits, retention/deletion policy and secret-managed credentials. MQTT site topic as in README; broker boundary is not implemented in the reference service.
