# Architecture

Three nodes: wheel sensor (rotation/runout sample), stand gauge (manual pressure and fixture force sample), ESP32-S3 hub (collects BLE telemetry; optional Wi-Fi forwarding). Each role is physically distinct to make its measurement location meaningful. The starter C firmware files only define integration points; no sensor drivers or radio integration are present. Current executable vertical slice is the portable codec and local REST API, not an end-to-end hardware system.

Trust boundary: all readings are untrusted observations; app/API must not imply safe/unsafe verdict. Offline sensor readings remain local to the hub; no cloud service is implemented. A future cloud deployment requires authenticated TLS MQTT, per-device ACLs, bounded retention, deletion workflow, and operator consent. Failure of a node produces unavailable data; hub failure stops collection but cannot affect bicycle operation. No remote control or actuator path exists.
