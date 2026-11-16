# Architecture and trust boundaries

Data path: HX711 bridge measurement and explicit button action at Kitchen Dock -> authenticated deployment transport (not implemented; prototype assumes trusted LAN) -> Home Hub event validation/store -> local REST consumer. Storage Sentinel independently advertises temperature and leak context over BLE. Hub joins device identity only after local operator enrollment (enrollment UI is not implemented). Collection App is a display scaffold; staff-confirmed handoff must be entered manually.

Offline behavior: nodes should buffer at most 64 monotonically sequenced events and retry with bounded exponential backoff; this policy is specified but not implemented in the sample firmware. Hub restart must mark readings stale until new telemetry arrives. Queue overflow is a data gap, never a guessed value. A mass threshold can provide a reminder only; it cannot command machinery.

Power: Dock USB-C 5V from certified external isolated supply, regulated 3.3V; estimated electronics load is not measured. Load-cell mechanics must carry expected container mass with a stable platform and overload stop. Sentinel coin-cell runtime and radio peak capability are unverified. Hub uses Pi Zero 2 W with certified 5V supply. No mains-connected circuitry exists in the design.

Privacy/security: no ambient sensing, personal identity, camera, or audio. Prototype stores events in process memory only. For a deployment: local SQLite encryption/retention policy, consentful enrollment, TLS, per-device credentials/ACLs, signed OTA and revocation are prerequisites. Do not expose prototype API or broker to the internet.

Known engineering gaps: no KiCad native schematics/PCB; no power-budget measurement; no firmware SDK build; no MQTT broker adapter; no production mobile app; no real training dataset; no RF, EMC, ingress, oil compatibility, or user validation. This package is an implementable concept, not a build-ready appliance.
