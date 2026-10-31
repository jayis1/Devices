# Architecture and assembly notes

Reference topology: two battery/USB sentinel nodes communicate to a Raspberry Pi Zero 2 W gateway over region-matched RFM95W-class sub-GHz radios. Node firmware examples intentionally isolate board HAL work; GPIO/I2C assignments in source are proposed board-level signals, not confirmed module pin capabilities. Confirm ESP32-C3 module datasheet and radio/SPI pin allocation before PCB capture.

Gateway services: local MQTT broker with ACLs, subscriber/validator, SQLite append-only observations, and local status UI. During internet loss, the gateway remains authoritative for local observations and retries cloud sync. A gateway replacement requires fresh enrollment; do not trust arbitrary node IDs.

Assembly order: prototype sensor buses on dev boards; validate sensor address and temperature against a reference; measure regulator and radio peak currents; characterize enclosure/antenna range in situ; then create native schematics and PCB. No battery/runtime figures are claimed. Use protected cell assemblies with a qualified charger/protection circuit; current BOM is incomplete as a production design.

Pin notes (provisional): cabinet I2C SDA=GPIO6, SCL=GPIO7; reed=GPIO4 with external pull-up; radio SPI pin mapping TBD per board layout. Utility I2C SDA=GPIO6, SCL=GPIO7. Resolve conflicts and boot-strapping constraints before implementation. Schematics are not yet native KiCad files.
