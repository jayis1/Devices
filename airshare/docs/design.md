# AirShare prototype design and acceptance notes

## Node inventory

Hub: ESP32-S3-DevKitC-1; USB 5 V input, onboard 3.3 V regulation, Wi-Fi/BLE aggregation. Prototype pin allocation: GPIO4 I2C SDA, GPIO5 I2C SCL, GPIO8 status LED. Avoid claiming board-level electrical validation; verify exact devkit revision and pin availability before assembly.

Room Sentinel: ESP32-C3 module, SCD41 CO2 sensor (I2C 0x62), SHT31 temperature/RH (0x44), USB 5 V supply with regulated 3.3 V sensor rail. GPIO4 SDA, GPIO5 SCL. SCD41 measurement current peaks require regulator margin; sensor self-heating and enclosure airflow affect accuracy. Mount away from supply vents and direct sunlight.

Doorway Context Node: nRF52840 module on regulated 3.3 V USB, GPIO P0.11 reed switch to ground with internal pull-up, debounce 30 ms. Door state is context, not ventilation proof.

Radio: BLE 5.0 local star; Wi-Fi only at hub. No radio range or coexistence claims without site survey. Power: USB-powered prototype nodes; no battery-life claim. Use certified modules and respect local radio requirements.

## Shared event contract

JSON fields: schema_version=1, node_id (opaque configured ID), sequence (uint32), captured_at (UTC ISO-8601), co2_ppm (0..10000, optional), temperature_c (-40..85, optional), relative_humidity_pct (0..100, optional), door_open (boolean, optional). At least one reading field required. Reject malformed or out-of-range values. Retained MQTT commands are not used; telemetry is observation-only. Production deployment requires per-device authenticated provisioning, TLS and topic ACLs; current demo API has no authentication.

## Bill of materials (prototype estimates)

See per-node CSVs. Prices are rough USD planning values, exclude enclosure, assembly, and calibration. No PCB layout, antenna matching, battery, surge protection, or certification included. Before fabrication, reconcile exact module variants, pinouts, supply limits, and footprints against vendor datasheets.

## Failure behavior and acceptance

A missing node or stale timestamp displays UNKNOWN, never “good air.” Hub reconnect reports current state; no event-history guarantee. Sensor errors are omitted and surfaced as unavailable. No actuator or life-safety function. Test duplicates, malformed ranges, service restart, broker loss, and stale status. Hardware calibration, environmental chamber, BLE coexistence, enclosure, EMC, and building commissioning tests remain unperformed.
