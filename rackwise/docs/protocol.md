# Wire protocol v1

Transport: MQTT 3.1.1/5 over TLS, site broker. Topic `rackwise/v1/sites/{site_id}/devices/{device_id}/telemetry`; device identity in topic must equal payload identity. Device publish-only ACL. No broker command topics are defined.

UTF-8 JSON, maximum 1024 bytes, one object per message:

```json
{"schema":"rackwise.telemetry.v1","device_id":"rack-a","seq":12,"observed_at":"2026-04-10T12:00:00Z","kind":"rack","payload":{"bay_count":4,"states":["occupied","clear","unknown","clear"]}}
```

`seq` is unsigned 32-bit monotonic per boot-persistent device counter; persist before transmission. On counter wrap, increment `boot_id` (UUID) and restart sequence at zero; dedup key is `(device_id,boot_id,seq)`. `observed_at` is RFC3339 UTC, informational; gateway receipt time governs freshness. Rack payload permits 1–8 bay states from `occupied|clear|unknown`. Context payload kind `weather` has numeric `temperature_c`, `relative_humidity_pct`, `pressure_hpa`, each nullable. Units are explicit. Reject unknown schema, topic mismatch, invalid units/ranges, oversized payload, and duplicate/older sequence within the same boot. Broker QoS 1 may redeliver; consumers must deduplicate. Nodes retry with bounded exponential backoff (1–60 s) and retain at most 32 newest messages; overflow drops oldest while marking local diagnostics. No end-to-end payload signature; TLS and broker ACLs are required. OTA, provisioning, and cryptographic key rotation are not implemented.

Example states express sensor interpretation only; a missing beam edge is `unknown`, never an implicit clear. Do not use this protocol as security, billing, or enforcement evidence.