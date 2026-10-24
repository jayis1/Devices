# Local API v1

`GET /health` returns `{"status":"ok"}`. `GET /inventory` returns latest telemetry keyed by node ID. `POST /events` accepts JSON `{node_id:int, sequence:int, type:int, value:int}` and rejects duplicate sequence keys with HTTP 409. API is unauthenticated for loopback/LAN prototype use only; bind to localhost or an isolated trusted LAN. Never expose publicly. MQTT bridge is optional and not enabled by default; remote deployment requires TLS, credentials, topic ACLs, and authenticated API access.
