# Architecture and construction notes

## Nodes

| Node | SoC | Inputs | Local behavior | Supply |
|---|---|---|---|---|
| Gateway | ESP32-C6 module | Wi-Fi; MQTT uplink | event buffer, API, status aggregation | regulated 5 V USB to 3.3 V |
| Stall Sentinel | ESP32-C3 module | door reed GPIO4; HX711 load-cell ADC GPIO5/6 | coarse door/presence context; no identity inference | isolated 5 V USB |
| Water Sentinel | ESP32-C3 module | DS18B20 GPIO4; clamp-on pulse input GPIO5 | surface temperature and flow pulse counts | isolated 5 V USB |

Do not place electronics or mains wiring in wet zones. A qualified installer must assess electrical protection, ingress rating, mounting and local codes. No device controls valves, heaters, locks, or pumps. Door closed + load threshold is not proof of occupancy; outputs are explicitly contextual, never a safety or availability guarantee.

## Wiring and power

All GPIO are 3.3 V only. Stall: reed between GPIO4 and ground, internal/external 10 kΩ pull-up; HX711 DOUT→GPIO5 and SCK→GPIO6, module powered per its verified datasheet (logic output must not exceed 3.3 V). Water: DS18B20 data→GPIO4 with 4.7 kΩ pull-up to 3.3 V; pulse output open-collector to GPIO5 with 10 kΩ pull-up. Each node uses a certified 5 V USB adapter and a 3.3 V regulator sized for ESP radio peaks; budget at least 500 mA per node, verify actual board requirements. Never connect an unknown sensor output directly; check level and isolation first.

## Trust and failure boundaries

Nodes send only coarse events. Provision unique credentials during installation; use WPA2/3 and TLS with certificate validation, per-node MQTT publish ACLs and gateway-only API access. Rotate/revoke credentials on reset or ownership transfer. No secrets in firmware defaults or repository. The prototype does not implement provisioning, TLS, or ACLs: it must not be exposed beyond loopback until those controls exist. On network loss, preserve last state with stale marker and bounded queue; on reboot, mark state unknown until fresh readings. Sensor faults produce unknown, not available. Staff inspection remains authoritative.

## Data lifecycle

No audio, images, identity, or individual movement logs. Aggregate status locally; raw event retention target 24 hours, configurable shorter; deletion on site reset. Notifications show facility status only. Synthetic model data is not evidence of prediction quality. ML is advisory and disabled when unavailable.

## Readiness

Concept-level wiring only. No KiCad ERC, PCB, electrical, enclosure, RF, wet-location, or hardware tests have been performed. Component footprints/variants and safety suitability require engineering review before procurement/build.
