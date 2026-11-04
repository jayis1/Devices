# ChoreCue local API intent

Proposed local API, not yet implemented:

- `GET /v1/tasks`: list household-configured tasks.
- `POST /v1/events`: submit one protocol-v1 event; enforce household authorization and deduplicate `(node,seq)`.
- `GET /v1/summary`: return household-approved aggregate completion counts for a selected interval.

Prototype backend currently exposes only a Python `validate_event` function and tests. No server, authentication, persistence, or MQTT broker integration is present. Do not expose it as a service.
