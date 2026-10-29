# Architecture and assembly notes

Install observer next to the printer's existing status LED using a light pipe/phototransistor; validate thresholds per printer and leave its wiring electrically isolated. Door reed switch is optional context only. SHT31 sits inside the filament enclosure, away from heater/air outlet. Nodes are USB powered in this reference configuration to avoid unsupported battery-life claims.

Gateway scans enrolled BLE nodes, maps numeric node IDs to local site topics, and forwards telemetry to the LAN broker. Keep a local last-seen cache; show unknown after 600 s without a valid frame. On lost broker, retain latest state in RAM and retry with bounded exponential backoff; do not queue unbounded history. On gateway reboot, show all nodes unknown until received again.

Trust boundaries: sensor nodes -> gateway -> MQTT broker -> API -> browser/mobile. Production deployment needs per-node identity, authenticated BLE pairing, TLS MQTT, broker ACLs, API authentication, signed OTA, rate limits, and secret rotation. Those are requirements, not implemented features here. Device IDs should be randomized per site and must not encode member identity.

Bring-up: verify regulator and IO voltage, connect programmer/debug interface, test the sensor inputs with isolated low-voltage sources, then validate each radio link. No mains connection is allowed. Check printer vendor warranty before attaching anything. No fabrication, emissions, calibration, or safety testing has been performed.
