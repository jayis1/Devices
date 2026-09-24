# HygieneGuard API

Author: jayis1

The FastAPI reference requires `X-API-Token` for writes when `HYGIENEGUARD_API_TOKEN` is set. It is an in-memory prototype; deploy persistence, TLS termination, MQTT authentication, rate limiting, and audit policy before exposure.

| Method | Path | Purpose |
|---|---|---|
| GET | `/health` | service and known-node count |
| GET | `/stations` | latest station state, including stale/unknown |
| POST | `/telemetry` | validate and retain a node observation |
| GET | `/cards` | explainable refill/maintenance cards |

Example telemetry: `{"node_id":"sink-kitchen-01","soap_g":180.0,"flow_ml":350.0,"timestamp":1735689600}`. The API does not accept names, biometric identifiers, recordings, or medical information.