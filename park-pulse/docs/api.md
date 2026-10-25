# API

MQTT topic `parkpulse/v1/{site_id}/{node_id}/telemetry`; payload JSON fields version, sequence, type, value, unit, observed_at. FastAPI `GET /health` and `GET /sites/{site_id}/latest` expose latest validated state. Demo service has no auth and binds localhost only; production requires TLS, operator authentication and per-site authorization. Unknown node, unsupported type, malformed values, and stale sequence are rejected. Broker connection/retry is deployment-adapter work; local gateway remains authoritative.
