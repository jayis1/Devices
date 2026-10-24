# Local API

Prototype FastAPI routes (implementation skeleton):

- `GET /health` returns process health.
- `GET /queue` returns aggregate count, configured service rate, and wait band; response is advisory and may be unavailable.
- `POST /events` accepts a validated event object with node ID, sequence, type and observed timestamp.

Production deployment must authenticate and authorize every request, enforce payload/rate limits, use TLS for any remote link, restrict CORS, avoid sensitive logs, and implement retention/deletion. MQTT integration is optional and not enabled by default; configure unique device credentials and TLS before enabling. Never use the estimate for triage or priority decisions.
