# API prototype

`POST /events` accepts protocol v1 JSON fields documented in `protocol.md`, validates bounds and deduplicates `(node_id,seq)`. `GET /events` returns most recent prototype events. Development implementation uses in-memory storage, has no authentication, persistence, MQTT subscriber, or production rate limiting. Bind only to localhost in development. Production must add user/site authorization, TLS termination, persistent storage, topic ACLs, retention/deletion, and audit controls before deployment.
