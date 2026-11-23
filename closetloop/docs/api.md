# API

POST `/events` accepts version 1 event JSON and returns `accepted`; duplicate event IDs are idempotent. GET `/events?limit=100` returns newest first; limit is 1–500. SQLite is prototype persistence, no accounts/auth/tenant isolation are implemented. Do not expose this server to an untrusted network. MQTT transport is not implemented in this reference API.
