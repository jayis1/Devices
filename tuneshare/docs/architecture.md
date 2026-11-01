# Architecture and validation boundary

The gateway is the only coordinator. Climate and rack peripherals report bounded event messages over BLE; the button dock emits a human confirmation event. Core state and deduplication are intended to reside locally at the gateway. Cloud MQTT is optional telemetry only. A disconnected cloud cannot disable local status.

The climate observation is contextual, not an instrument-preservation guarantee. Sensor position, airflow, enclosure self-heating, and calibration affect readings. The rack optical sensors indicate beam interruption only; staff must confirm the instrument itself is present and usable. No actuator is present, so failures cannot trigger autonomous environmental changes.

Threats include unauthorized BLE enrollment, spoofed events, physical access, and exposed cloud credentials. The concept requires BLE bonding and per-device identity; prototype code does not implement enrollment, encryption policy, signed firmware updates, or backend authorization. Keep the API private to the local network until these are implemented.

Validation status: only host protocol bounds and C syntax are eligible for local automated checks. No native KiCad design, PCB, RF test, ESP-IDF build, environmental chamber test, privacy/security audit, or field evaluation is provided. No fabrication or product-readiness claim is made.
