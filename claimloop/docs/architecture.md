# Architecture and operations

ClaimLoop is a venue-operated lost-property workflow, not a smart locker or identity system. Physical intake and claim decisions remain staff-controlled. Tray/shelf sensors add context only; they cannot determine item ownership or guarantee that property is present.

Nodes: intake tray (load cell plus explicit staff confirmation); shelf-door node (open/closed context); gateway (local API relay). The current firmware sources are portable event-policy examples with external HAL functions, not complete board projects. The backend accepts event JSON into SQLite. Mobile UI is a visual stub with no authentication or live data connection. ML script tests pipeline output generation using seeded synthetic data; it trains no model.

Local operation does not depend on internet. An API failure or power loss falls back to paper forms. The initial stack has no MQTT broker, cloud deployment, mobile integration, BLE transport, provisioning, or authorization implementation. These are intentionally not claimed complete.

Assembly sequence: verify exact dev boards and pin straps; build USB-only prototype; calibrate load cell with known masses; test debounce and duplicate handling; evaluate enclosure stability; then have an electrical/RF review. Stop on unstable weight, disconnected sensor, or invalid value; staff may use the ordinary paper workflow. No network command can move or unlock anything because the design contains no actuator.

Targets: host C syntax and backend contract tests only. Target firmware builds, electrical checks, RF measurements, privacy/security review, and venue trials remain outstanding.
