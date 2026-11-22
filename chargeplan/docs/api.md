# API reference

Base URL defaults to loopback at deployment. `GET /health` returns status and in-memory event count. `POST /events` accepts the protocol event JSON and returns accepted/duplicate; unsupported kinds are rejected. Pydantic rejects unsupported versions and out-of-range values with HTTP 422. `GET /events?limit=N` returns at most N events (1–500), newest insertion order last. This unauthenticated development API is not for network exposure; add authentication, TLS, durable storage, and per-node authorization before deployment.
