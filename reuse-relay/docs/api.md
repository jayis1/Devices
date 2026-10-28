# Local API v1 (demonstrator)

Base URL `http://<hub>:8000`; production must use HTTPS and authenticated site authorization.

- `GET /health` → `{ "status": "ok" }`
- `GET /offers` → current non-expired offers (coarse category, status, expiry, opaque ID only)
- `POST /offers` accepts `{ "category": "furniture", "expires_in_hours": 24 }`, returns generated offer
- `POST /offers/{id}/picked-up` marks human-confirmed handoff

Categories: furniture, kitchenware, clothing, books, other. No free-text, location, identity, image, or contact details. Demo API uses in-memory storage and has no authentication; do not expose to an untrusted network. Presence telemetry is deliberately separate from offer creation.
