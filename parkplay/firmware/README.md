# IDF firmware integration seam

Target: ESP-IDF 5.x, ESP32-C3-MINI-1. GPIO assignments are provisional; verify carrier board pin availability. Each production node must implement a sensor-specific task, call the common encoder, and advertise exactly 16 bytes over BLE. Hardware drivers, pairing, authenticated telemetry, watchdog/OTA and target builds are not included in this prototype.

`common/protocol.c` is host-buildable and tested separately. Do not treat the examples as flashable firmware.
