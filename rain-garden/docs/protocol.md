# Protocol v1

Transport: MQTT QoS 1 on `raingarden/v1/{site_id}/{node_id}/telemetry`; UTF-8 JSON payload. Numeric units: `soil_proxy_pct` 0–100 calibrated ADC proxy; `ponding_mm` 0–4000; `rain_mm` cumulative since reset; `battery_v` volts. Timestamp is RFC3339 UTC. Sequence is uint32 and wraps modulo 2^32; receiver retains a bounded duplicate window per node. Unsupported version and malformed/range-invalid values are rejected. QoS 1 may redeliver; consumers must be idempotent. No commands are defined.

Example: `{"version":1,"node_id":"soil-01","seq":12,"timestamp":"2026-06-01T12:00:00Z","kind":"soil","values":{"soil_proxy_pct":41.2,"battery_v":4.91}}`.

Production payload authentication/encryption is delegated to MQTT TLS and unique credentials (not implemented here); never trust public broker traffic.
