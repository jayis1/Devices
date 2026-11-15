# API

POST /events accepts JSON `{v,node,seq,kind,ts_ms,value}`. Supported kinds: rack_mass_g, return_passage. Duplicate/out-of-order sequence returns 409; unsupported kind or stale/future timestamp returns 422. GET /summary returns latest telemetry. POST /staff-actions accepts `{"action":"wash_started|wash_completed|rack_reset"}`. Current demonstration is in-memory, unauthenticated, and not suitable for deployment; add identity/authorization, persistent storage, bounds, and retention policy before use.
