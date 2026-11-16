# API

Prototype FastAPI endpoints: GET `/health`; GET `/events?limit=1..1000`; POST `/events` with v=1, node, seq, ts, kind and optional book fields. Invalid schema returns 422; out-of-range query returns 400; duplicate returns accepted=false. Data is in process memory only and server is intended for loopback/trusted-LAN development. There is no authentication, durable database, MQTT client, TLS, rate limiting, or object-level authorization: do not expose it to the internet.
