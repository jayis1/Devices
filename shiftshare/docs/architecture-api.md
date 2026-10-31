# Architecture and API

Gateway is authoritative for freshness and site state. BLE nodes emit versioned observations; gateway labels old data unknown after 15 minutes. Optional API accepts JSON `{v,node_id,seq,kind,ts_utc,values}` at `POST /v1/observations`; `GET /health` is a prototype liveness endpoint. There is no MQTT broker integration, authentication, persistence, TLS deployment, or public endpoint configuration in this prototype. Do not expose it to a network.

Cloud integration, if later implemented, must use TLS, per-site credentials, scoped topics, bounded retention, revocation, and explicit consent. No cloud dependency for local status.
