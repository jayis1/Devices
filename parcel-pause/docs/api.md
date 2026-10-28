# Local API
`POST /readings` JSON `{\"node_id\":2,\"node_type\":1,\"sequence\":1,\"age_ms\":0,\"value\":1,\"status\":0}`. Returns 202 for accepted data; rejects out-of-range values. `GET /health` returns service state. Prototype has no user authentication; bind locally only, never expose publicly. MQTT subscriber, durable database, account ACL, and TLS termination are deployment work.
