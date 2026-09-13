# API

All `/v1/*` endpoints require `Authorization: Bearer <MAINTAINSYNC_API_TOKEN>`.

- `GET /health` returns service time and status.
- `GET /v1/assets` returns local asset state and latest evidence.
- `POST /v1/events` accepts `{node_id, asset_id, kind, data}` and records an event.
- `GET /v1/cards` returns ranked maintenance cards.
- `POST /v1/interlocks/request` accepts `{node_id, command, ttl_s, local_key_confirmed}`. It rejects any request without local key confirmation and returns policy-pending only; radio delivery is hub-controlled.

MQTT topics: `maintainsync/<home>/node/<id>/telemetry`, `/event`, `/command`, and `/ack`. TLS client certificates are recommended. Do not publish secrets or imagery in MQTT payloads.
