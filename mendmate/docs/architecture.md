# Architecture and safety contract

## Roles and boundary
Hub (Raspberry Pi Zero 2 W) is the local coordinator and API host. ESP32-C3 swatch reader measures relative reflectance in a shielded, fixed-geometry optical cup. ESP32-C3 tool station estimates thread-tray mass and drawer state. There are no motors or hazardous outputs. Garment identity, repair description, and fabric composition are user inputs; sensors do not infer them.

## Electrical contract
3.3 V logic throughout. Reader: AS7341 I²C on GPIO4/5, scan button GPIO3, LED GPIO8 through resistor. Station: HX711 DOUT/SCK GPIO6/7 and drawer contact GPIO3. Both boards accept regulated 5 V USB input and use a module-compatible 3.3 V rail. Hub: 5 V USB. Per-node adapter design target 5 V/1 A; hub 5 V/2 A. These are conservative supply targets, not measured consumption. Validate module pin mapping/strap constraints, sensor breakout pullups, optical repeatability, HX711 calibration, and radio peak current before board design.

## Operating and failure modes
Sampling is explicit. On missing sensor, out-of-range value, or invalid CRC, discard the sample and report fault/staleness; never fabricate a value. On LAN or hub loss, nodes may buffer at most 64 telemetry events, dropping oldest telemetry on overflow; no critical action depends on the network. On reboot, counters reset only with a new boot session and hub deduplicates by `(device_id, boot_id, seq)`. Current demo backend is in-memory and loses state on restart. Thread weight is an estimate, not an assurance that a matching thread is usable. User always decides repair suitability and operates tools safely.

## Privacy, security, and validation boundary
No images, audio, tracking, or cloud transmission. Prototype API and MQTT interfaces lack authentication/TLS/commissioning and are unsafe for untrusted networks; isolate on a private LAN. Production requires device-specific credentials, broker ACL, TLS, authenticated user access, data export/deletion, signed updates and recovery. No personal data is needed. This design has not had an electrical, optical, RF, mechanical, or textile engineering review. Wiring references are not KiCad schematics or fabrication documentation.
