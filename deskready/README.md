# DeskReady — community computer-lab station readiness

DeskReady helps library and community-center staff see whether shared computer stations are available, powered, and due for a human check. It does not inspect screens, capture keystrokes, identify users, or remotely control mains power. Prototype reference design; not a certified electrical or availability monitor.

## User value and boundaries

A staff member can check a local dashboard before sending a visitor to a station. Each desk node observes only low-voltage USB power and a staff-operated ready button; the room node observes room temperature and humidity; a gateway collects signed-format versioned events and serves the dashboard. “Ready” is an operator declaration, not proof that a computer or accessibility feature works. A human must verify the station. Internet loss does not affect local reporting.

Nodes: Gateway (Raspberry Pi Zero 2 W), Desk Node (ESP32-C3 module, one per station), Room Node (ESP32-C3 + SHT31). Nodes use BLE GATT to the gateway; Wi-Fi/Ethernet local dashboard is optional. Prototype BLE transport has no production provisioning/authentication implemented; use a physically controlled lab network and do not expose it to the internet.

## Hardware and connections

Desk Node: ESP32-C3-MINI-1 (3.3 V module); USB-C 5 V input through a protected 5 V-to-3.3 V regulator; INA219 measures an isolated, current-limited 5 V USB test load (never inline with a computer's USB port); GPIO4 ready button to ground with internal pull-up; GPIO5 optional isolated low-voltage USB-present comparator output; UART0 programming pads, reset/boot buttons, decoupling, and test points. Do not connect the measurement fixture to a computer without an independently validated isolation/current-limit design. Prototype firmware currently exercises the button input and event framing only.

Room Node: ESP32-C3-MINI-1, SHT31-DIS-B temperature/humidity sensor on I2C (SDA GPIO6, SCL GPIO7; 4.7 kΩ pull-ups to 3.3 V), UART0 programming pads, reset/boot controls. Gateway: Raspberry Pi Zero 2 W, 5 V USB supply, BLE; no custom mains wiring. GPIO assignments are proposed and must be checked against the exact module revision and board before layout.

Power: USB 5 V input; 3.3 V regulated logic. No batteries, actuators, or mains switching. Provide ESD protection at external connectors and a fused/current-limited sensor fixture in any physical implementation. Antennas require keep-out per module datasheet. Enclosure is indoor, dry, ventilated, non-plenum; not a fire-alarm or environmental-control device.

## Protocol and data handling

JSON event v1: `{\"v\":1,\"node\":\"desk-01\",\"seq\":7,\"kind\":\"checkin\",\"ready\":true,\"uptime_s\":120}`. Sequence is monotonic per boot; gateway deduplicates `(node, boot_id, seq)`, rejects unsupported versions and stale data, and labels stale after 10 minutes. Sample units are °C and %RH. The prototype shared C header defines a bounded event struct and encoder; it is not cryptographic authentication. Do not treat node IDs as authorization. No personal data is collected. Local database retention is 30 days; no cloud upload by default.

## Software

- `firmware/desk-node/main.c` and `firmware/room-node/main.c`: ESP-IDF application entry points and event framing; hardware drivers are marked as integration points.
- `software/dashboard`: FastAPI local service with health, event-ingest and station-state endpoints.
- `software/ml-pipeline`: deterministic baseline script using synthetic fixtures only; it reports stale/availability bands and makes no predictive-performance claim.
- `software/mobile-app`: React Native Expo operator-view stub; API URL is configurable and screens are not a tested production client.
- `schematic/README.md`: connection-level design notes, not native KiCad schematics. PCB, ERC, radio, electrical safety, and physical measurements remain unverified.

## Bring-up and checks

Run dashboard tests with `python -m unittest discover -s software/dashboard -v`; run the baseline with `python software/ml-pipeline/baseline.py`; compile protocol smoke test with `cc -std=c11 -Wall -Wextra -Werror firmware/tests/protocol_test.c -o /tmp/deskready-protocol && /tmp/deskready-protocol`. Firmware requires ESP-IDF and board-level validation. No cloud deployment is supplied.

## BOM (prototype estimates, availability not checked)

See `hardware/bom/*.csv`. Exact module/board footprints and regulator/ESD selections must be confirmed against the purchased revision and datasheets before fabrication. This is a prototype specification, not a manufacturing release.

## Failure behavior and limitations

Gateway outage leaves nodes locally powered but data unavailable; nodes do not actuate anything. BLE loss produces stale status, not a false ready indication. Power loss clears in-memory event counters; gateway dedupe must use a fresh boot identifier in a production implementation. No ML inference is used for station decisions. Do not use for emergency egress, safety-critical access, or sole proof of workstation function.
