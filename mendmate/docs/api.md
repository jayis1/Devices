# Local API v1

Prototype routes (no authentication; bind only to trusted LAN):

- `GET /health` → `{status:"ok"}`
- `POST /v1/events` accepts an envelope as defined in `protocol.md`, validates values and stores latest event by device in memory; returns `{accepted:true}`. Duplicate/older sequences return 409.
- `GET /v1/status` → latest node events with age seconds; missing/stale nodes remain missing/stale.
- `POST /v1/repairs` accepts `{garment, damage, fabric_label?}` as user-provided plain text (length limits apply); returns locally generated id. This prototype does not provide durable storage or user accounts.

Optional MQTT adapter in `software/dashboard/mqtt_bridge.py` subscribes to `mendmate/v1/+/telemetry` using TLS CA validation and the same event validation path. Configure `MENDMATE_MQTT_HOST`, `MENDMATE_MQTT_CA`, and device credentials using a broker ACL that restricts each device to its own publish topic. The bridge is not automatically started by FastAPI; deployment must supervise it explicitly. Do not expose API publicly or use API data for safety decisions. Production deployment must authenticate users, authorize object access, encrypt transport, limit request rate, and support data deletion.
