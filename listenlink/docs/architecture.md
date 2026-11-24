# ListenLink architecture

## Nodes and wiring

- Venue Hub: ESP32-C6-MINI-1, 3.3 V; Wi-Fi MQTT uplink and BLE scan, USB-C 5 V input to a regulated 3.3 V rail. UART0 GPIO16/17 for programming/debug; I²C GPIO6 SDA/GPIO7 SCL to optional local status display. Reserve strapping pins and verify exact module pinout before layout.
- Receiver Dock: ESP32-C3-MINI-1, 3.3 V; 8 normally-open bay switches to MCP23017 inputs GPA0..GPA7, each with 10 kΩ pull-up to 3.3 V; MCP23017 SDA/SCL GPIO8/9; USB 5 V power; UART0 GPIO20/21 debug. Switch closure indicates presence only.
- Room Beacon: nRF52840 module, CR2032, 3 V; tactile toggle input P0.11 with internal pull-up; status LED P0.13; BLE advertising. Battery chemistry, peak TX current and regulator/module requirements must be checked against module datasheet; avoid direct coin-cell load if peak-current testing shows brownout.

Hub polls dock over BLE in the prototype. A production deployment should use authenticated BLE GATT provisioning and a wired or authenticated wireless link; do not trust unauthenticated advertisements. MQTT example topic `listenlink/v1/site/{site}/room/{room}/state`; payload contains schema_version, timestamp, transmitter_on, receiver_count and node sequence, no identity. Broker ACL restricts each hub to its site topic. API: GET /health and GET /v1/readiness; POST /v1/readings validates bounded fields. TLS cert validation and per-device credentials are deployment requirements, not implemented here.

## Behavior

Local last-known state is retained while WAN is down; stale after 120 seconds and displayed as unknown, never ready. Duplicate or lower sequence readings are rejected. Node loss produces unknown. Human controls and venue audio operation remain independent. Firmware update signing, secure enrollment, persistent replay counters, durable event storage and broker reconnect policy are production work, not implemented.

## Power / verification

USB 5 V hub/dock supplies should be certified enclosed adapters. No node switches venue audio or mains. Measure radio burst current and brownout with the selected modules before battery sizing. Validate every module pin against its current datasheet. No PCB layout, KiCad ERC, RF certification, or hardware bench tests are claimed.
