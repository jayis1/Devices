# Architecture and requirements

## Traceability
- Observe stored water level: tank raw range measurement → gateway telemetry → `/api/v1/telemetry` → app freshness card. Validate by reference dipstick and controlled known levels.
- Observe collection: tip count → gateway rainfall event → history; validate gauge volume per tip.
- Observe demand: pulse count → usage context; calibrate against measured volume.
- Preserve local operation: leaf sampling and gateway display continue without internet; cloud is optional.

## Failure handling
Missing radio samples become stale after three expected reporting intervals. Gateway loss means app status is explicitly unavailable; leaf buffers are bounded and overwrite oldest samples. Sensor timeout/implausible jump is a quality fault, not zero. Reboots increment boot ID and reset sequence. Clock is not trusted for freshness until gateway receipt time; monotonic uptime is diagnostic only. No valve or pump is commanded.

## Security maturity
Prototype radio frames use CRC for accidental corruption only, not authentication or confidentiality. Production requires per-node unique keys, authenticated encryption, replay windows, enrollment/revocation, TLS to broker/API, topic ACLs, signed OTA and rollback protections. Do not deploy scaffold on exposed networks.

## Validation needed
Bench-test sensor calibration, wet-enclosure condensation, cable transient protection, battery capacity across seasons, LoRa regional compliance and gateway coverage. Overflow routing must be installed independently and remain passive.
