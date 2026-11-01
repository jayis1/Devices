# Protocol v1

Transport: BLE advertisement, local star reception at gateway. UTF-8 JSON payload, max 160 bytes. No fragmentation. Gateway ignores malformed, oversized, unknown-version, out-of-range, and duplicate messages.

| Kind | Direction | Value/unit | Suggested cadence |
|---|---|---|---|
| stock | rack → gateway | estimated aggregate count / items | on change, otherwise 5 min |
| return_fill | dock → gateway | normalized 0..100 / percent | on change, otherwise 5 min |
| inspection | beacon → gateway | button event / event | on press |

Envelope: `{"v":1,"id":"node-01","node":"rack","seq":1,"ts_ms":1234,"kind":"stock","value":12,"unit":"items"}`. `id` is provisioned random node identifier; `node` is one of `rack`, `dock`, `inspection`; `seq` is unsigned 32-bit monotonic sequence with wrap; `ts_ms` is monotonic uptime, not UTC. Gateway receive time is authoritative for ordering across nodes. Repeated `(id,seq)` is ignored. Duplicate/out-of-order events must not overwrite a newer sequence. No ACK/retry at link layer; nodes periodically re-advertise latest state. Inspection events are best effort and require manual record fallback.

This unencrypted prototype has no authentication, replay resistance, or remote command channel. CRC can detect accidental corruption but does not authenticate a node. Production security is a prerequisite, not an optional hardening step.