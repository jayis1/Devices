# TuneShare interface contract

BLE GATT is the node-to-gateway transport. Each application JSON object (UTF-8, at most 256 bytes) has `v:1`, a provisioned opaque `node_id`, uint32 `seq`, `ts` (UTC epoch seconds or null), `kind`, and `data`. Climate kind: `data.temp_c` and `data.rh_pct`; rack kind: `data.occupied_mask` uint16; checkin kind: `data.action` equal to `confirm` or `clear`. Reject unknown versions, invalid ranges (temperature -20..80 C, humidity 0..100%), and malformed or oversized payloads. Gateway deduplicates per node_id/seq; stale after 24 hours. Wall-clock timestamps can be null offline.

Gateway publishes optional MQTT telemetry to `tuneshare/v1/{site_id}/telemetry`; credentials must be unique per gateway and topic-restricted. Use TLS. The demo scaffold does not implement TLS provisioning or authorization; never expose its broker publicly. API JSON follows the same envelope. App status is advisory and shows stale/offline explicitly. No actuator commands are defined.
