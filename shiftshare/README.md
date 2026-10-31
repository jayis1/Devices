# ShiftShare — community rest-site readiness

ShiftShare helps libraries, shelters, and community centers tell night and rotating-shift workers which designated rest rooms are open, quiet, and equipped before they make a trip. It reports room-level context only; it does not identify visitors, reserve seats, or claim that a space is safe or suitable for sleep.

## User and measurable goal

Mara works rotating shifts and needs a quiet place to decompress between shifts without traveling to a closed facility. Staff currently rely on calls or walk-by checks. Target: reduce unnecessary trips to unavailable designated rooms, measured as staff-confirmed unavailable visits per operating week before/after deployment. This is an evaluation target, not a demonstrated result.

## Topology

```text
[Room node: door + ambient sound level] --BLE--> [Site gateway + local display]
[Supply node: cot/blanket stock, door context] --BLE--> [Site gateway]
[Staff check-in button] -----------------------> [Gateway] --> optional HTTPS API
                                                      | local status page / LEDs
                                                      + optional React Native client
```

The separated room node observes sound pressure level (no audio recording); supply node measures designated clean-blanket stock by load cell. This avoids visitor sensing and makes readiness useful without a cloud. Gateway aggregates BLE advertisements, serves a local page and an accessible three-state LED/e-ink indicator; staff confirm open/closed and cleaning status with physical buttons. BLE link is for prototype indoor coverage; site survey determines placement.

## Nodes and electrical design

Prototype component choices (verify exact module revisions and current before procurement):

| Node | SoC / parts | Interfaces and prototype pins | Power / safe state |
|---|---|---|---|
| Gateway | Raspberry Pi Pico W (RP2040/Wi-Fi), SSD1306 128x64 I2C display, 3 buttons, RGB LED | I2C SDA GP4/SCL GP5; buttons GP10-12 with pull-ups; LED GP13 via resistor; USB 5 V | USB 5 V; display blank on fault; no locks or actuators |
| Room sentinel | Seeed XIAO nRF52840, VEML6030 lux (optional), analog sound-level module MAX4466 | I2C SDA P0.04/SCL P0.05 where exposed; sound envelope analog P0.02/ADC; BLE 5 | USB 5 V or protected Li-ion module; publish unknown if sensor absent; no microphone samples stored |
| Supply dock | XIAO nRF52840, NAU7802 load-cell ADC + 5 kg load cell, reed switch | I2C shared bus; reed GPIO P0.03; BLE 5 | USB 5 V; zero output on calibration/ADC fault; stock value never interpreted as hygiene/safety |

Pin names above are board-level intent, not a checked schematic. Confirm XIAO pin mapping against the exact revision before layout. No mains, motor, access control, medical function, or life-safety function. Battery charging/protection is deliberately delegated to a certified protected module; enclosure, PCB, EMC/RF, and regulatory engineering remain outstanding.

## Protocol and operation

Prototype payload schema: `{v:1,node_id:string,seq:uint32,kind:string,ts_utc:int|null,values:object}`. Units are dBA-equivalent relative level (calibrate against a reference meter), lux, blanket count estimate, and open/closed/unknown. Gateway rejects unknown versions, duplicate sequence values and out-of-range measurements; expired observations older than 15 minutes become unknown. No remote actuation exists. Pairing requires a staff-held physical button and local enrollment window. BLE advertisements are not authenticated in this prototype; deploy only on a controlled test network until authenticated pairing is implemented. Cloud is optional and receives aggregates only with site consent.

## Privacy, accessibility, degraded operation

No camera, audio recording, occupancy counting, identity, location history, or visitor identifier. Local readings retained for 24 hours, then overwritten; cloud retention disabled by default. Status is advisory, never an assurance of access, cleanliness, quiet, or safety. Color is redundant with text/icons and tactile buttons; high contrast and screen-reader labels are required in the app. Without internet, gateway keeps last readings and marks stale data unknown. If a sensor fails or link is lost, that room is unavailable/unknown rather than guessed. Staff override is explicit and expires after the site-defined interval.

## BOM and build status

Per-node BOMs are in `hardware/bom/`. Schematic pin/net intent is in `schematic/`; native KiCad capture and PCB layout are not yet supplied. Firmware folders contain portable protocol/reference code, not board SDK builds. Backend and ML pipeline are prototype scaffolds; ML uses synthetic data and has no validated predictive model. Mobile app is a minimal interface stub.

## Verification status

Prototype documentation and source only. No hardware assembled, calibrated, field-tested, or certified. Pin assignments and component interoperability require datasheet and board-revision review. Build/test instructions: `python3 -m unittest discover -s software/dashboard/tests`; `python3 -m py_compile software/dashboard/app.py software/ml-pipeline/train.py`; compile shared C with `gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only firmware/common/protocol.c`.
