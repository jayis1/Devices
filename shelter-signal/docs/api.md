# Demo API

Documented by jayis1.

`software/dashboard/main.py` offers `GET /health`, `POST /events`, and `GET /events`. Event JSON: `node_id` (1..65535), `kind` (light/contact/approach/heartbeat), `sequence` (uint32), `age_s` (uint32), `value` (int32). It validates bounds and holds at most 500 events in memory. No authentication or durable storage exists; bind to loopback only. Production requires user/site authorization, TLS, retention controls and broker ACLs.
