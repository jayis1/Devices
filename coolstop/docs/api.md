# API

Base URL defaults to `http://127.0.0.1:8000`; bind only to a trusted site LAN after configuring network controls. Authentication is not implemented, so do not expose outside a trusted isolated LAN.

- `GET /health` returns service health.
- `POST /telemetry` accepts protocol-v1 JSON and returns `{"accepted":true}`; invalid payloads are rejected.
- `GET /telemetry/latest` returns the latest valid reading per node from local SQLite.

MQTT subscriber (optional dependency) consumes `coolstop/v1/+/+/telemetry` and submits the same validated payload. Duplicate reports are idempotently stored by node ID (latest record wins); seq is retained for consumers. No delete/retention endpoint yet. Operators must apply local data-retention policy. Payloads contain no user identity.
