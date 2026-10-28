# Architecture, pin map, and safety

## Node pin assignments

| Node | GPIO / bus | Assignment |
|---|---|---|
| Probe ESP32-C3-MINI-1 | GPIO6 | TMP117 SDA, I²C address 0x48 |
| Probe | GPIO7 | TMP117 SCL; 4.7 kΩ pull-ups to 3.3 V |
| Door ESP32-C3-MINI-1 | GPIO4 | Normally-closed reed contact to GND; pull-up; open wire interpreted as fault/open |
| Hub ESP32-S3-WROOM-1 | GPIO8 | low-side NPN buzzer driver with base resistor; boot default off |
| Hub | GPIO9 | LED through current-limiting resistor |

Names refer to module GPIOs; confirm the selected carrier board exposes them and that its boot configuration permits use. No mains voltage, compressor switching, or food-contact electrical exposure. I²C is local to the probe, 3.3 V only. Final schematic, layout, enclosure, ESD, and RF design are not included.

## Data path and failure modes

Probe and door emit records over BLE advertising. Hub validates length/range, timestamps reception, detects duplicate sequence values, maintains per-node freshness, applies deterministic configurable threshold and stale-sensor alarms, and buffers records locally. Optional MQTT uplink publishes sanitized telemetry; a backend stores/serves records; the mobile scaffold reads the API. Cloud or phone outage cannot disable the local alarm. Hub outage means no alarm: retain independent manual thermometer checks. Sensor/battery failure is reported as unknown, not safe. No remote actuation exists.

## Threat and safety boundary

Prototype advertisements are spoofable and replayable. Deploy only in a controlled evaluation; do not store customer or health data. MQTT must use TLS, device/user authentication and topic ACLs in production. A threshold crossing is an operational prompt, not a determination that food is safe or unsafe. Follow local law and site procedures; human operator makes disposition decisions. Production release requires risk assessment, security review, calibrated probe validation, and regulatory review.
