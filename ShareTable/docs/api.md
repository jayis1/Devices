# API

`POST /v1/telemetry` accepts the protocol-v1 object plus authenticated site context. Duplicate `(site,node,seq)` returns 202 without duplicate storage. Invalid schema returns 422; unauthenticated requests return 401. `GET /health` reports process status only, not broker or data completeness. Production deployment requires TLS termination, authenticated MQTT, per-site authorization, rate limiting, and a durable database; this example is not production-ready.
