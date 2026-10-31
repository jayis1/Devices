# Prototype API

`GET /health` returns status ok. `POST /v1/observations` accepts node_id (1..2^32-1), sequence (0..65535), state (unknown/idle/active), optional sample_mA (0..65534), timezone-aware observed_at; returns 202, duplicate returns 200, invalid input 422. `GET /v1/stations` lists latest states; records older than 90 seconds become unknown. `GET /v1/stations/{node_id}` returns latest or 404.

Prototype accepts unauthenticated HTTP and uses local SQLite; bind to loopback/trusted LAN only. MQTT subscriber, authentication, authorization, retention/deletion controls, rate limits and TLS termination are not implemented. Do not expose publicly.