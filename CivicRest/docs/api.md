# API (prototype)
POST /v1/events accepts {site,node_id,kind,value}; GET /health. Intended MQTT topics civicrest/{site}/{node_id}/telemetry and /status. Authentication, TLS, per-site authorization, broker ACLs and retention are not implemented; do not expose publicly. Do not send visitor identifiers.
