# QuietHours — shared-home quiet-time coordination

QuietHours helps roommates and families avoid accidental noise during agreed quiet windows. It is an advisory, local-first system: a room node estimates sound level (never records audio), a doorway node provides an explicit quiet-mode button, and a hub shows current decibel bands and sends gentle opt-in reminders. It is not a sound meter for legal, workplace, medical, or tenancy enforcement.

## Differentiation
Unlike QuietPath, which provides coarse outdoor route context, QuietHours coordinates consent-based quiet windows inside one shared home. Unlike SoundNest, it neither classifies sound nor performs masking; no audio is stored or transmitted. Its primary data is short-window A-weighted level estimates and user-entered schedules.

## Nodes and build outline
- Hub: Raspberry Pi Zero 2 W, 5 V USB-C supply, local Mosquitto + FastAPI service + browser dashboard. Coordinates local policy and retains events locally.
- Room Sentinel: ESP32-C3-MINI-1, INMP441 digital microphone, 1.3-inch SSD1306 OLED, tactile acknowledge button; BLE 5.0 to hub. Firmware computes 1 s RMS and transmits only quantized dBA-equivalent estimates. Microphone response requires calibration; readings are approximate.
- Doorway Beacon: nRF52840 module, tactile quiet-mode/override buttons, RGB LED, BLE 5.0. Coin-cell operation is not claimed; use protected 500 mAh LiPo and USB charging module.

Power: low-voltage USB supplies only. No mains switching or autonomous sound suppression. Each node continues displaying its last explicitly selected mode during hub loss; no reminders are sent until reconnection. A physical override always silences prompts for 60 minutes. Data is retained locally for 7 days and is deletable in the dashboard.

## Status and limitations
This is an engineering prototype specification, not a validated product. The schematic directory contains pin-level wiring/netlist descriptions, not fabrication-ready KiCad schematics. BOMs identify proposed parts and require supplier/package verification. No acoustic calibration, regulatory, accessibility, or coexistence testing has been performed. Do not use readings to settle disputes or make compliance claims.

## Interfaces
Room estimates use protocol v1 JSON: `{\"v\":1,\"id\":\"room-1\",\"seq\":1,\"t\":0,\"kind\":\"level\",\"db10\":420}`. `db10` is an approximate dBA-equivalent in tenths, not a calibrated measurement. BLE writes are authenticated through a one-time provisioning code; production key storage and rotation remain unimplemented. Hub MQTT binds localhost by default; remote access must be placed behind authenticated TLS.

## Software
- `firmware/`: ESP-IDF C room node and Zephyr C doorway beacon skeletons.
- `software/dashboard/`: FastAPI local event endpoint.
- `software/ml-pipeline/`: optional, explicitly synthetic threshold demo; no model-performance claims.
- `software/mobile-app/`: React Native concept stub; pairing/notifications are not implemented.
- `docs/`: architecture, protocol, and safety notes.

## Validation
Run `python -m unittest discover -s software/dashboard/tests` and `python software/ml-pipeline/train.py --demo`. Firmware target builds require ESP-IDF and Zephyr SDK respectively and were not run in this environment. No KiCad ERC or physical tests have been run.
