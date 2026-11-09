# RackWise architecture

## Scope
RackWise estimates public bicycle-rack availability from anonymous beam interruption. Pilot target: sheltered, staff-maintained site with two rack observer nodes, each monitoring up to four marked bays, one contextual weather node, and a coordinating local gateway. The success measure for a later field pilot is agreement of five-minute availability bands (`many` >= 4, `some` 1–3, `full` 0, `unknown` when stale) with staff spot-checks; this repository contains no field data and makes no accuracy claim.

## Topology

```text
Rack observer A --Wi-Fi/MQTT--+
Rack observer B --Wi-Fi/MQTT--+--> Site gateway (Mosquitto + API + aggregation)
Weather node ----Wi-Fi/MQTT--+             | local read-only API
                                            +--> mobile display
```

Each observer debounces transitions for 500 ms, emits a report every 60 s and immediately after stable changes. The gateway deduplicates by device sequence, retains last state, and marks a node unknown after 180 s without a valid report. API consumers receive timestamps and per-node freshness. Five-minute aggregation is advisory only. A cloud relay is optional and out of this prototype's scope.

## Power / installation
Use isolated commercial 5 V SELV USB supplies and local 3.3 V regulators. Do not route mains into the enclosure or connect to building wiring. Sensor leads are low-voltage, strain-relieved, and mounted so they cannot create a trip or snag hazard. Protect inputs with series resistance and ESD protection in a production design. The reference prototype omits PCB, enclosure, surge, ingress, EMC, and antenna studies. Install only with site permission; do not obstruct accessible paths.

## Failure behavior
Broken/blocked sensor channel => unknown, not occupied/clear. Wi-Fi/broker loss => local sensor state remains local, gateway marks stale and API reports unavailable. Gateway restart => all data unknown until refreshed. Clock unavailable => freshness uses gateway receipt time; event timestamp is retained only as supplied metadata. Duplicate reports do not change counts. Sensor removal or calibration changes require staff verification. No commands or actuators exist.

## Privacy and security
No images, audio, MAC-derived visitor identity, phone identifiers, or bike identifiers are collected. Use rotating unique device credentials, TLS MQTT, and least-privilege publish topics; prohibit anonymous broker access. API prototype is unauthenticated and intended only for loopback/private-network testing; add site authentication and authorization before exposure. Aggregate data should be coarsened and retention bounded as described in README. No public live count endpoint is enabled by default.

## Readiness
Software reference code is testable on a host. Schematics are wiring references only. Firmware peripheral integration, physical calibration, weather sealing, coverage, radio coexistence, accessibility review, security review, and field validation remain open.