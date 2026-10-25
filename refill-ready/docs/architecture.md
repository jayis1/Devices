# Architecture and build boundary

The useful outcome is a trustworthy maintenance signal, not automatic water-quality assurance. A fixture node measures flow pulses and a leak contact; a gateway owns time, local queue, and uplink; a staff path-check node supports periodic manual route observations. Each node is independently useful; no visitor sensing is present.

```text
flow/leak node -> BLE link -> site gateway -> MQTT/TLS -> ingestion API -> SQLite -> staff client
manual path-check node -----------^                  \\-> bounded offline queue
```

Event states: `normal`, `suspect`, `inspection_required`, `confirmed_by_staff`, `cleared_by_staff`. Flow thresholds are configured per fixture after calibration, never global defaults. Missing/stale readings become `inspection_required`; they do not imply safe availability. Gateway queues at most 256 frames and discards oldest with an explicit loss counter on overflow.

Components are listed in `hardware/bom/`. Pin and supply contracts are in `hardware.md`. Prototype firmware exposes HAL seams; target builds require vendor SDK integration. No production installation or water contact is approved by this reference design.
