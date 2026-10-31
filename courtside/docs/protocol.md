# Protocol v1

UTF-8 JSON carried by BLE GATT (notify/read characteristic); gateway forwards equivalent object to backend. Fields required: version integer `1`; site_id and node_id strings (1–48 chars); seq unsigned integer; observed_at UTC RFC3339 string; kind enum `surface_observation|inspection|maintenance_report`; data object. Max frame 512 bytes. Units: wetness_raw ADC counts (0–32767), temperature_c degrees Celsius, humidity_pct percent. Missing sensor values are null, never zero-substituted. Duplicate `(node_id,seq)` is idempotently ignored; unknown versions/kinds and malformed timestamps are rejected. Gateway preserves original timestamp and records receipt time separately. Frames do not include cryptographic authentication; production requires authenticated BLE enrollment plus TLS gateway transport and replay protection.

Example:

```json
{"version":1,"site_id":"park-17","node_id":"surface-a","seq":42,"observed_at":"2026-05-20T12:00:00Z","kind":"surface_observation","data":{"wetness_raw":812,"temperature_c":17.4,"humidity_pct":61.0}}
```

Rules: wetness above site-calibrated threshold yields `review`; data older than 30 minutes yields `stale`; rules are prompts for human inspection, not playability judgments. WAN loss: gateway queues up to 1000 records and retries with bounded exponential backoff. Full queue drops oldest telemetry but retains inspection/report records where possible; it indicates data loss locally.
