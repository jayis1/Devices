# Local API

Author: jayis1

Run `uvicorn main:app --host 127.0.0.1 --port 8000` from `software/dashboard`. Set `SENSORYSYNC_API_TOKEN` to a non-default value.

| Route | Purpose |
|---|---|
| `GET /health` | readiness and author metadata |
| `POST /v1/telemetry` | authenticated, validated telemetry ingestion |
| `GET /v1/cards` | authenticated overload/staleness cards |
| `POST /v1/commands` | authenticated bounded-comfort command request |

The prototype holds events in memory; deployment requires SQLite persistence and a real MQTT adapter. Commands refuse unknown node IDs, values outside 0..1, and expired requests. This is not a direct hardware-control API.
