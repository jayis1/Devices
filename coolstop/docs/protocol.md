# Protocol v1

Transport: MQTT over the site LAN, one telemetry publish per node to `coolstop/v1/<site_id>/<node_id>/telemetry`. The Linux host bridges MQTT to HTTP; the gateway displays node freshness and does not bridge radio. QoS 1, non-retained telemetry; consumers must tolerate duplicates. TLS and unique per-node broker credentials are deployment requirements but provisioning/ACL configuration is not supplied here. Never expose an unauthenticated broker publicly.

UTF-8 JSON bounded to 1024 bytes. Required keys: `version` integer (=1), `node_id` nonempty string, `seq` nonnegative integer, `timestamp` RFC3339 UTC string, `kind` one of `climate` or `supply`, `values` object. Climate values: `temperature_c` finite number [-20,80], `relative_humidity_pct` finite number [0,100]. Supply values: `mass_g` finite number [0,100000]. Unknown versions/kinds and invalid ranges are rejected. Sequence persistence across reboot is not implemented in firmware; deployment must treat rebooted counters carefully. Timestamp is sampled from network time; when clock unavailable, do not publish an invented UTC time.

Example: `{"version":1,"node_id":"climate-01","seq":42,"timestamp":"2026-06-20T14:05:00Z","kind":"climate","values":{"temperature_c":29.4,"relative_humidity_pct":38.2}}`

This is a prototype, not an authenticated end-to-end protocol implementation. MQTT transport ACL/TLS and node enrollment remain deployment work.
