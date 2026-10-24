# Architecture and responsibility boundaries

The system has one site hub, at least two independent air sentinels, and one surface probe. Nodes report on a regional LoRa star; the hub timestamps/validates and persists records locally, and optionally publishes TLS-protected MQTT telemetry to FastAPI. The mobile UI reads API site summaries; no remote actuation exists.

```text
STM32L0/SHT45+BME280 x2 --LoRa--> Pico W + RTC + bounded store --TLS MQTT--> API/SQLite
STM32L0/PT1000+wetness ---LoRa-/                                  \-> RN view stub
```

Priority is deterministic advisory logic only: wetness > 0 and surface temperature <= 2°C is “inspect promptly”; stale or missing data is “unknown,” never safe. Operator confirmation and local winter policy govern response. Store node ID, sequence, readings, and timestamps; no personal data. Offline cache maximum 7 days; at capacity, retain newest readings and expose dropped count. Hub restart must not replay control commands (none are supported). Planned but unimplemented threat controls: authenticated enrollment, per-node keys, authenticated encryption/replay protection, broker ACLs, TLS certificate rotation, signed firmware updates.

Bring-up requires region check, enclosure/antenna validation, RTD calibration, soak test, RF tests, battery thermal and charge validation, and multiple freeze/thaw pilot observations. No KiCad ERC, PCB, vendor SDK build, or field trial has been performed. Current is a reference design only.
