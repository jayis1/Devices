# Architecture

Shelf nodes produce calibrated mass deltas; the app asks a person to identify or dismiss the change. Fridge node adds temperature and door-open context only. BLE bonded nodes send versioned events to a Pi gateway. The gateway persists bounded events locally and optionally forwards MQTT over TLS; API clients see household-scoped inventory and events. Offline mode retains queued telemetry; local edits remain authoritative. No automatic item recognition, cooking advice, food-safety decision, or appliance control.

Unknowns: radio range, calibration drift, battery life, condensation protection, EMC, and usability require testing. Prototype is advisory only.
