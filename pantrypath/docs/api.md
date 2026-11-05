# API contract

Prototype `POST /api/v1/events` JSON: `{node_id, sequence, observed_at, event_type, value}`; uniqueness by node/sequence makes inserts idempotent. Keep broker private. Production requires verified TLS, client authentication, per-device topic ACLs, bounded requests, user authorization, and retention/deletion policy. This scaffold is not production-secure.
