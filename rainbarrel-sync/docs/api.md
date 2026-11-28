# HTTP API v1

`POST /api/v1/telemetry` accepts JSON `{node_id, kind, value, unit, sequence, observed_at}`. Values are numeric and bounded; node ID is 1–32 chars, kind/unit are enumerated, and sequence is a nonnegative integer. Prototype endpoint stores latest values in process memory and is not authenticated or durable. Do not expose publicly.

`GET /api/v1/status` returns the latest received telemetry. MQTT production topics are planned as `rainbarrel/v1/{site}/{node}/telemetry`; TLS and per-node publish ACLs are mandatory before use.
