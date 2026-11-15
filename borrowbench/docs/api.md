# API

`POST /events` accepts v1 event JSON; returns `{accepted,duplicate}`. Errors: 413 oversized payload, 422 schema validation. `GET /events?limit=100` returns newest events; limit 1..500. This prototype has no authentication, user/tenant authorization, cloud broker, rate limiting, or TLS termination. Bind only to a trusted isolated LAN for development; never expose publicly. A production deployment must add authenticated staff accounts, site-level authorization, TLS, audit/deletion controls and broker ACLs.
