# ClaimLoop contracts

## Event API
`POST /events` accepts JSON `{version:1,node_id,sequence,kind,value,monotonic_ms}`. `kind` is `tray_weight_g`, `shelf_door`, or `staff_intake`; units are integer grams for weight/intake and 0/1 for closed/open. `node_id` is 1–23 ASCII letters, digits, `_` or `-`; sequence/time are unsigned 32-bit values. Duplicate `(node_id, sequence)` returns 409; invalid schema/value returns 422. `GET /events` returns at most the newest 100 observations. No claim codes or item descriptions are part of this telemetry API.

## Threat/privacy boundary
The reference API is unauthenticated and intended only for loopback/test networks. Do not expose it to a venue or internet until adding TLS, staff authentication, per-device authorization, rate limits, and retention/deletion controls. The prototype has no secrets or embedded keys. Restrict network access at the host firewall. No public object-level inventory endpoint exists.
