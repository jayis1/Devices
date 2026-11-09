# API notes

Proposed endpoints: `POST /v1/events` accepts a validated telemetry event; `GET /v1/households/{id}/events` returns authorized household events. Production requires OAuth/device credentials, household authorization, rate limits, TLS and bounded retention. No operational backend is included in this scaffold.
