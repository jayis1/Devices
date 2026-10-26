# Local API

Base URL defaults to `http://127.0.0.1:8000`; bind only to loopback in the prototype.

| Method | Path | Result |
|---|---|---|
| GET | `/health` | process health |
| GET | `/v1/outlets` | known outlet states; unknown/offline explicit |
| POST | `/v1/outlets/{node_id}/inspection` | record staff inspection note (prototype local auth TODO) |

JSON input is bounded and validated; errors use standard HTTP status codes. Production deployment requires TLS, authentication/authorization by site, rate limits, retention limits, and audit logging. Do not expose the sample service to the public internet.