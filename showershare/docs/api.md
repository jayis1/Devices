# API prototype

`POST /v1/events` accepts JSON protocol events (max body 512 bytes; v1 and bounded values required). `GET /health` returns process health only. MQTT deployments should bridge `showershare/v1/+/+/telemetry` into the same validator with TLS and per-node publish ACLs; MQTT is not enabled by this minimal prototype. No authentication is implemented, so server binds to loopback by default and is for local tests only. Never expose publicly as-is.
