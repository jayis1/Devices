# Local API

Prototype FastAPI service (`software/dashboard/main.py`):

- `GET /health` returns status and prototype mode.
- `POST /events` validates protocol fields and rejects duplicates by `(node,seq)`.
- `GET /events?limit=100` returns latest in-memory events; limit is 1..500.

Run with `cd software/dashboard && python -m pip install -r requirements.txt && uvicorn main:app --host 127.0.0.1`. The current service has no authentication, persistence, MQTT subscriber, TLS termination, rate limiting, or ownership model. Bind only to loopback for local tests. Do not expose it to an untrusted network.
