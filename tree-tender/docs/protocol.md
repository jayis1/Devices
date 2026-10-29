# Observation protocol v1

Transport prototype: BLE manufacturer-data advertisement for local observations, then MQTT topic `treetender/v1/<site>/<node>/observation` over TLS from gateway. Payload is UTF-8 JSON at MQTT boundary; production BLE encoding should use bounded CBOR. Example:

```json
{"v":1,"node":"probe-01","seq":17,"ts":1793293200,"kind":"soil","soil_pct":38.2,"temp_c":14.1,"rh_pct":62.0}
```

Weather example uses `kind: weather`, `rain_mm` (accumulated since last report), `temp_c`, `rh_pct`, and `pressure_hpa`. Units are explicit; timestamps are UTC epoch seconds. Node IDs are site-assigned opaque slugs. Sequence is unsigned 16-bit modulo; backend uniqueness uses site/node/sequence/timestamp. Duplicate messages are idempotent. Range validation is 0–100% soil/RH, -40–85 C, 300–1200 hPa, and nonnegative rain. Invalid fields reject the record.

Version 1 does not claim application-layer authentication or BLE replay resistance. MQTT TLS and unique broker credentials are deployment requirements. Do not expose the broker publicly or use real secrets in examples. Future protocol version must specify authenticated enrollment, cryptographic message integrity, replay window, and key rotation before field use.
