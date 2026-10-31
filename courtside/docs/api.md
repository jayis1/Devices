# Demo API

`GET /health` returns demo readiness. `GET /observations` lists newest-first in-memory events. `POST /observations` accepts protocol v1 JSON and returns `{accepted: bool, duplicate: bool}`. Invalid payloads receive HTTP 422; duplicate `(node_id,seq)` gets accepted=false. No auth, persistence, MQTT consumer, TLS termination, rate limiting, or tenant isolation is included; bind only to a trusted local test environment.
