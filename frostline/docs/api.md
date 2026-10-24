# API

| Method | Path | Behavior |
|---|---|---|
| GET | /health | Liveness response |
| POST | /api/v1/telemetry | Validate and store one observation; duplicate site/node/sequence returns 409 |
| GET | /api/v1/sites/{site_id}/latest | Return up to 32 newest observations or 404 |

JSON fields follow the `Telemetry` Pydantic model in `software/dashboard/main.py`; temperature units are °C, RH percent, battery mV, timestamp ISO-8601 UTC. Demo persists SQLite locally. MQTT ingestion is not yet connected. No API authentication, authorization, TLS termination, or rate limiting is configured by this demo; bind only to loopback until those controls are added. Never expose on a public network as-is.
