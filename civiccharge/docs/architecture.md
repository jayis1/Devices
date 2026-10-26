# Architecture

Three or more outlet monitors associate with one Raspberry Pi Zero 2 W gateway over BLE 5.0. The system is read-only with respect to mains: no switching or load control. Monitors publish bounded measurements and explicit validity flags; gateway maps provisioned node IDs to site labels and persists last-known status. Stale/missing values become unknown, never healthy. Optional MQTT bridges telemetry outbound over TLS; local operation does not depend on internet.

Hardware separation: mains conductors and sensor front-end are designed only by qualified electrical professionals using an approved isolated measurement module and enclosure. The module's output must be SELV and documented before connecting to nRF52840 ADC. The prototype netlists are interface specifications, not wiring instructions for mains.

Failure behavior: monitor restart increments persistent sequence or is re-enrolled; gateway restart reloads SQLite and marks state stale until fresh telemetry arrives; BLE loss produces offline status; internet loss preserves local API; invalid ADC/sensor input sets sensor-valid false. No command path can energize an outlet.
