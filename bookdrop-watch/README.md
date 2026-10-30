# BookDrop Watch

BookDrop Watch helps public-library facilities teams catch a jammed or full after-hours returns chute before returned books pile up outdoors. It is for library staff and patrons using unattended exterior book drops; the system reports equipment condition, not whether a book was returned or checked in.

## Distinction and value

Unlike LibraryLoop (anonymous seat availability) or ParcelPause (shared package-room occupancy), this system monitors a mechanically distinct library return chute and its receiving-bin capacity. It does not identify patrons, scan books, or alter library circulation records. The measurable target is fewer staff-hours spent on avoidable chute checks and fewer returns exposed to weather; those outcomes require a field pilot and are not claimed as proven.

## Architecture

```text
Chute Sentinel (XIAO nRF52840: beam + flap switch) --BLE--+
                                                         +--> Library Gateway (ESP32-C6)
Bin Scale Node (XIAO nRF52840: load cell) --------------BLE--+       | local queue/status
                                                                   +--Wi-Fi/MQTT TLS--> FastAPI
                                                                                 React Native staff app
```

Two independent battery nodes report into a continuously powered site gateway. The beam and flap switch detect passage/activity and possible blockage; a load cell estimates bin fill. They are contextual signals only: staff verify every alert. Gateway operation and queued events continue without internet. There are no actuators.

## Hardware and interfaces

Reference prototype choices, not fabrication-ready schematics. Verify module revisions, sensor ranges, enclosure ingress, and actual chute geometry before installation.

| Node | SoC / interfaces | Pin intent | Power |
|---|---|---|---|
| Gateway | ESP32-C6-WROOM-1; BLE + Wi-Fi; USB-C 5 V | I2C OLED SDA GPIO6/SCL GPIO7; status LED GPIO15 | certified 5 V USB supply |
| Chute Sentinel | Seeed XIAO nRF52840; BLE | break-beam receiver D0; flap reed switch D1; service button D2 | protected 1S 1000 mAh cell |
| Bin Scale | Seeed XIAO nRF52840; BLE | HX711 DOUT D0, SCK D1; tare button D2 | protected 1S 1000 mAh cell |

Use isolated, low-voltage sensors; strain-relieve wiring and keep all electronics outside the book path. HX711 connects to a suitably rated load cell; secure the bin mechanically so its load cannot pinch wiring. Charge cells only with a protected, correctly rated charger and do not charge unattended. No mains wiring is part of this design.

### Power estimates

Design targets, not measured values. Chute node: 1000 mAh / 0.035 mA average = 28,571 h (about 3.3 ideal years); derate to 12 months for cold, cell aging, radio retries, and sensor duty. Scale node target 0.20 mA average = 5,000 h (208 ideal days); service target 90 days. Gateway is continuously powered. Validate sleep current on assembled hardware.

## Protocol and operation

BLE application payload is UTF-8 JSON, max 160 bytes: `{"v":1,"node":"chute-01","seq":42,"kind":"passage","value":1}`. `kind` is `passage`, `flap`, `bin_mass_g`, or `heartbeat`; `seq` is uint32 and monotonically increasing per node. Gateway deduplicates `(node,seq)`, rejects unsupported versions and oversized payloads, timestamps receipt, and buffers locally. Missing or stale readings mean UNKNOWN, never clear/available. MQTT topic: `bookdropwatch/v1/{site_id}/{node_id}/telemetry`; backend accepts the same envelope via `POST /telemetry`.

Prototype BLE telemetry is not authenticated. Production requires per-install device credentials, bonded/encrypted BLE, authenticated MQTT over TLS, API authentication, and signed firmware update support before deployment. No patron identifiers, images, audio, or circulation data are collected. Local event retention target is 30 days, configurable by library policy.

## Software and maturity

- `firmware/common/`: portable C payload size/version validation; host test.
- `firmware/chute-sentinel/`, `firmware/bin-scale/`: illustrative sampling loops; board SDK integration remains future work.
- `schematic/`: wiring/pin intent only; no KiCad fabrication design is claimed.
- `hardware/bom/`: estimated node-level parts and costs.
- `software/dashboard/`: FastAPI local prototype and tests; MQTT integration is not active.
- `software/ml-pipeline/`: reproducible synthetic-data baseline, demonstration only; rules remain the operational fallback.
- `software/mobile-app/`: minimal React Native concept stub, not install-ready.

Run C checks: `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/bookdrop-test && /tmp/bookdrop-test`. Python API test: `python3 -m unittest discover -s software/dashboard/tests`. ML smoke test: `python3 software/ml-pipeline/train.py`. No physical build, RF, weather, load-cell calibration, or field validation has been performed. The app is not type-checked here. This is an advisory prototype, not a circulation-system integration or guarantee of secure unattended operation.

## Bring-up and failure behavior

1. Assemble and calibrate the load cell with known masses; test beam alignment and flap switch without installing the electronics in the return path.
2. Flash node firmware using the vendor XIAO nRF52840 board package and confirm BLE advertisements in a local test.
3. Configure gateway Wi-Fi/MQTT credentials via environment or local provisioning (never commit credentials); confirm offline buffering and duplicate suppression.
4. Establish a site-specific alert threshold with staff. Treat sensor disagreement, empty history, low battery, or stale reports as an inspection prompt.

Cloud loss does not interrupt sensing; gateway storage exhaustion must surface a local fault. Node or radio failure produces UNKNOWN. Staff maintain the chute using normal inspection procedures; this system never controls the chute or records individual users.
