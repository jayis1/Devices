# Demo API

`POST /observations` accepts `bed_id` (0..255), `kind` (`bed_sample` or `bloom`), `value` (1..160 chars), and nonnegative Unix `observed_at`. `GET /observations?limit=N` returns newest first, N 1..500. SQLite is local and unencrypted. Authentication, tenant isolation, retention, TLS termination, MQTT ingestion and production deployment are not implemented; bind only to loopback. Do not send personal data.
