# RouteReady — community climbing-wall route status

Status: reference prototype specification. Not field-tested, electrically validated, or safety-certified. Invented and maintained by [jayis1](https://github.com/jayis1).

RouteReady helps climbing-gym staff coordinate route-setting work and publish staff-declared wall-section status. It connects a local hub, wall-section beacon(s), and setter check-in dock. It does not inspect holds, assess structural integrity, or determine whether a route is safe to climb. The gym's inspection and closure procedures remain authoritative.

## Architecture

```text
Wall-section beacon×N ─BLE─┐
Setter check-in dock ─BLE───┼─> Gym gateway (RP2040/Pico W) ─Wi-Fi/MQTT─> FastAPI + SQLite
                            └───────────────────────────────────────────> React Native status client
```

Staff use a physical switch to declare a section open, setting, or closed; NFC tags identify a route record only when scanned by staff. The gateway is the coordinator. It caches recent states locally and marks disconnected nodes unknown. No actuators, cameras, microphones, visitor identifiers, or automatic safety decisions are present.

## Nodes and reference pin map

| Node | SoC / parts | Connections | Power |
|---|---|---|---|
| Gym gateway | Raspberry Pi Pico W (RP2040/CYW43439) | USB 5 V; 3V3 logic; UART0 GP0/GP1 debug | Certified isolated 5 V USB supply |
| Section beacon | ESP32-C3-MINI-1; 3-position maintained switch; status LED | switch GPIO4/5 with pull-ups; LED GPIO6 through 1 kΩ | 5 V USB, regulated 3V3 |
| Setter dock | ESP32-C3-MINI-1; PN532 NFC module; confirm button | I²C SDA GPIO4/SCL GPIO5; button GPIO6 pull-up | 5 V USB, regulated 3V3 |

Module pin mappings are reference assignments, not PCB netlists. Verify board pin availability, boot strapping, regulator burst capacity, decoupling, antenna clearance, and NFC module voltage before layout. Use protected low-voltage enclosures away from climbing surfaces and impact zones. No battery-life claim.

## Event protocol

JSON events (UTF-8, max 256 bytes): `{\"v\":1,\"node\":\"section-a\",\"seq\":42,\"kind\":\"section_state\",\"value\":\"closed\",\"utc_ms\":0}`. Values are enumerated and bounded: `section_state` = `open|setting|closed`; `route_checkin` = opaque route ID of 1–32 alphanumeric/dash characters; `battery_mv` integer 0–6000. Sequence is unsigned 32-bit; gateway deduplicates `(node,seq)`, rejects invalid versions/ranges, and treats clock zero as unknown. MQTT topic `routeready/<gym>/events`; production requires TLS and per-device credentials. Prototype BLE does not authenticate advertisements. No remote commands are supported.

## Software and data

`software/dashboard` is a local FastAPI event intake API with SQLite persistence and input validation. `software/ml-pipeline` produces a clearly synthetic baseline for estimating setter workload; no predictive performance is claimed, and it is not used for safety decisions. `software/mobile-app` is a read-only Expo/React Native status stub. Local events should be retained only for the gym's operational need and deleted on schedule; do not enter climber identities or health information.

## BOM and operational boundaries

See per-node CSVs in `hardware/bom/`. Cost figures are rough component estimates, excluding labor, enclosures, and compliance. No network or cloud dependency is required for staff to perform inspections. If the gateway or a beacon fails, the display must say unknown and staff revert to the gym's existing manual procedure. A software status is never an inspection, closure, or reopening authorization.

## Schematics, builds, and validation

`schematic/README.md` contains connection plans, not KiCad files or fabrication-ready designs. C code under `firmware/` is portable protocol/reference code, not target firmware. Run `python3 -m unittest -v` in `software/dashboard`, `gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/routeready-test && /tmp/routeready-test`, and `python3 software/ml-pipeline/train.py`. No target firmware, KiCad ERC, RF, mobile build, or physical gym test is claimed.

Invented and maintained by [jayis1](https://github.com/jayis1).
