# Architecture

## Nodes and boundary

Climate and Supply nodes sample local sensors and advertise signed/paired BLE GATT telemetry to the Site Gateway. The gateway validates bounds and freshness, stores a small ring buffer, computes explicit readiness flags, and optionally publishes JSON telemetry to MQTT over TLS. FastAPI exposes only site-scoped read APIs. Mobile UI is an operator view; it does not issue actuator commands.

```text
[SHT31 + SCD41] -> [Climate ESP32-C6] -- BLE --> [ESP32-S3 Site Gateway] -- TLS/MQTT --> [FastAPI]
[Load cell + HX711 + beam] -> [Supply ESP32-C6] -- BLE -->             \--> [local status]
```

Trust boundaries: sensor nodes are physically accessible and potentially replaceable; gateway owns enrollment and local aggregation; broker/API credentials are server-side environment secrets. Visitors are not authenticated or tracked. Site IDs are operator-assigned pseudonymous identifiers.

## Data flow

Sample every 60 seconds; include node ID, monotonically increasing sequence, quality bits and sampled values. Reject malformed/out-of-range readings and duplicate sequences. Mark data stale after five minutes. Gateway buffers at most 24 hours of compact readings, dropping oldest records when full. Backend stores latest site state and timestamp; retention is configurable, default 30 days. Local threshold rules define readiness; optional ML cannot override invalid/stale flags.

## Recovery

On boot, outputs remain inactive (no actuators). Nodes retry pairing with bounded backoff. Gateway restart reloads provisioned identities and validates sequence freshness. Cloud outage does not affect local display. Firmware update signing, secure boot, production identity provisioning and the actual BLE crypto integration are deployment work, not implemented here.
