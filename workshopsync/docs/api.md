# WorkshopSync local API v1

Author: jayis1

The dashboard binds to a private LAN. Set `WORKSHOPSYNC_API_TOKEN` and `WORKSHOPSYNC_MQTT_URL` as deployment environment variables; values are not committed.

| Method | Path | Auth | Purpose |
|---|---|---|---|
| GET | `/health` | no | process health and UTC time |
| POST | `/v1/telemetry` | bearer | validate/idempotently store telemetry |
| GET | `/v1/cards` | bearer | current readiness/advisory cards |
| POST | `/v1/acknowledgements` | bearer | record a user acknowledgement |

Telemetry body: `node_id` integer, `seq` non-negative integer, `epoch` non-negative integer, `kind` string (max 48), `quality` one of `ok/stale/fault/calibrating`, and numeric `metrics`. Acknowledgements do not grant operational permission and are retained as local audit records.

HTTP 401 means token failure, 409 means duplicate telemetry, 422 means schema validation failure. The API does not expose machine-control endpoints.