# API (prototype contract)

Backend will expose `GET /health` and `POST /api/v1/observations` accepting `{node_id:int, sequence:int, observed_at:ISO-8601, temperature_c:number|null, door_open:boolean|null, supply_v:number|null}`. Requests are bounded and validated; deployments must add authentication and per-site authorization before network exposure. MQTT topic convention: `cacheready/v1/{site_id}/{node_id}/telemetry`; use TLS and broker ACLs. No credentials or production security implementation included in this prototype.
