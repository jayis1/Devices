# ParkKeys HTTP reference

`POST /events` accepts the protocol v1 envelope as JSON. `GET /reports` returns accepted events in memory, newest first. This prototype is intentionally local-only: it has no authentication, durable storage, MQTT subscriber, retention policy, or production deployment configuration. Do not expose it to an untrusted network. Canonical event contract: `../../docs/protocol.md`.
