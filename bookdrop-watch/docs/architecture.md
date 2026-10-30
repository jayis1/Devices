# Design and operating notes

Three physical nodes: one USB-powered ESP32-C6 gateway and two battery BLE observers. Sensors are mounted without obstructing chute movement or book handling. Gateway queues locally and serves as the coordinator; cloud is optional. No actuator or personal-data collection exists. Fault, stale telemetry, conflicting sensors, and unknown bin mass all require staff inspection. Prototype components/pin assignments and power estimates are documented in the root device README; none have been validated on hardware.
