# Protocol v1

UTF-8 JSON, maximum 512 bytes. Required: v=1, node (1–32 ASCII chars), seq (uint32), ts (UTC RFC3339), kind, value (finite numeric for readings), unit, quality. Reject unknown versions, non-finite/out-of-range values, malformed timestamps, oversized payloads. Duplicates keyed by site/node/seq are idempotently ignored; sequence wrap requires node re-pair. BLE advertisements carry no private data; use connected encrypted BLE pairing in deployment. MQTT topic `sharetable/v1/{site_id}/{node_id}/telemetry`; never accept wildcard publish identity. Staff-entered thermometer observations must be labeled `staff_entered` and are not certification.

Example: `{"v":1,"node":"serve-01","seq":17,"ts":"2026-01-01T12:00:00Z","kind":"probe_reading","value":72.4,"unit":"C","quality":"staff_entered"}`
