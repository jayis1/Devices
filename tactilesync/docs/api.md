# API

The local dashboard binds to loopback by default.

- `GET /health` returns hub API health and whether an MQTT URL was configured.
- `GET /events` returns the bounded local event queue.
- `POST /events` accepts `{source_id, type, pattern, acknowledged}` from the authenticated local bridge.
- `POST /events/{index}/ack` records acknowledgement for a displayed event.

Place the API behind authenticated TLS termination before any non-loopback exposure. Do not expose the development server directly to the internet.
