# LibraryLoop — privacy-first shared-space availability

Designed by [jayis1](https://github.com/jayis1). LibraryLoop helps public libraries and community study rooms show which desks are genuinely available without cameras, phone tracking, or individual reservations. A student can check a quiet zone before crossing town; staff can see aggregate seat turnover and maintenance faults. The measurable pilot target is fewer wasted trips and a 10-minute reduction in staff walk-throughs per shift; neither outcome has been field measured. Its wedge versus commercial desk-booking apps is anonymous, offline-first, low-cost sensing for walk-in public spaces, not user accounts or reservations.

## Novelty and minimum useful install

The repository covers home environment, pantry inventory, and household routines, but not shared public study-space occupancy. This is not an extension of RoutineSync: LibraryLoop estimates aggregate desk availability without locating or identifying people or tagged belongings. Alternatives considered: a community tool-library network (more access-control risk), public restroom status (cleanliness sensing is difficult to validate), and repair-café queueing (multi-node sensing is less necessary). Minimum install is one hub plus one desk cluster (four seats); additional clusters make a building-wide map. Target bill of materials is about $90 for hub + one cluster, estimates only.

## Architecture

```text
Seat pads (4, local ADC) ──RS-485/Modbus RTU──┐
Doorway counter (ToF, anonymous direction) ──┼── Site Hub (RP2040 + ESP32-C6)
                                             │     local policy + bounded cache
                                             ├── optional MQTT/TLS → FastAPI
                                             └── React Native staff/visitor view
```

The doorway counter is advisory and excluded from seat counts; no images, audio, MAC addresses, or individual trajectories are collected. The hub aggregates counts and reports sensor faults. With Internet down the local display/API and sensing continue; cloud sync queues at most 24 hours, then drops oldest records with an explicit gap marker. If a sensor fails, its seats become “unknown,” never “free.” No reservation, access control, or safety-critical function exists.

## Nodes and wiring reference

| Node | SoC / components | Interfaces and pin map | Power / enclosure |
|---|---|---|---|
| Site Hub | Raspberry Pi Pico W (RP2040 + CYW43439), MAX3485E RS-485 transceiver, 0.96-in I²C status display | UART0 GP0 TX / GP1 RX to MAX3485 DI/RO; GP2 DE+/RE; I²C0 GP4 SDA/GP5 SCL; USB CDC provisioning. 120 Ω termination selectable at bus ends. | Certified 5 V USB supply; 3.3 V board rail. Locked ventilated wall enclosure; antenna clearance. |
| Seat Cluster ×N | RP2040 module, four FSR 402 pads + 10 kΩ dividers, ADS1115 (0x48), MAX3485E | I²C GP4/5; ADS1115 A0–A3 each read one divider; UART0 GP0/1; GP2 DE; node address by DIP switches. Pads use strain-relieved low-voltage leads. | 5 V SELV supply; 3.3 V rail. Under-desk enclosure with replaceable pads; no structural load-bearing use. |
| Doorway Counter | ESP32-C6-MINI-1, VL53L1X ToF (0x29), MAX3485E | I²C GPIO6/7; UART GPIO16/17; DE GPIO18. Mount overhead; direction estimates are discarded after aggregate update. | USB 5 V; indoor ceiling enclosure; avoid direct sunlight. Not a fire/egress counter. |

Conceptual netlists and connector assignments are in `schematic/`; they are not KiCad schematics or fabrication-ready designs. Confirm exact module pin availability, board variants, regulator capacity, ESD protection and antenna clearances before layout. All electronics are SELV; no mains switching or physical access control.

## Protocol and data contract

`firmware/common/protocol.h` is the shared C source of truth: fixed 20-byte little-endian frame, protocol version 1, message kind, node ID, sequence, occupied/valid bitmasks, uptime seconds, and CRC-16/CCITT-FALSE. `docs/protocol.md` defines offsets, units and Modbus register mapping. CRC detects accidental corruption; RS-485 is not encrypted or authenticated, so the building bus must remain physically protected. Do not expose it as a public network. MQTT topic is `libraryloop/v1/{site_id}/{node_id}/telemetry`; JSON fields are defined in `docs/api.md`. Telemetry is idempotently keyed by `(node_id, sequence)`; stale data after 90 seconds is unknown. No command topic is implemented.

## Power, BOM, and operating cost

Mains-powered nodes use certified external 5 V SELV adapters; board/regulator thermal design and EMC remain to be reviewed. No battery lifetime claim applies. BOM line-item estimates (USD, 2026-10-24, not supplier quotes) total about $18 hub, $18 four-seat cluster, and $15 doorway counter ($51 electronics for the minimal install). The target pilot BOM is about $90 after roughly $39 for certified adapters, enclosure, cabling, and mounting hardware; labor, tax, and shipping excluded. Some lines are generic estimates, not distributor quotes; re-quote and validate exact module variants before purchasing.

## Software and ML

FastAPI provides `/health`, `/v1/telemetry`, and aggregate `/v1/sites/{site_id}/status`. The reference API is an in-memory local demo; production authentication, site tenancy, persistent storage, TLS broker and retention/deletion controls are not implemented. `.env.example` contains placeholders only. The ML script uses generated synthetic occupancy data to compare a rolling-mean baseline and a threshold classifier; it demonstrates plumbing only, has no real-world training or accuracy claim, and must never determine whether a person is present. Deterministic seat state and freshness rules remain authoritative. Mobile app source is an interface stub, not a release build.

## Build and commissioning

1. Inspect the netlists, select known-good modules, and have the wiring reviewed before assembly. Do not install in an egress path.
2. Compile the portable protocol: `gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/libraryloop-test && /tmp/libraryloop-test`.
3. Target SDK firmware is illustrative and not an ESP-IDF/Pico SDK project; no target image is claimed.
4. Install `software/dashboard/requirements.txt`, then run tests from that directory with `python -m unittest discover -s . -v`.
5. Run synthetic demo: `python software/ml-pipeline/train.py --synthetic`.
6. Calibrate each pad unloaded, then with a repeatable 5 kg test weight; set thresholds above measured unloaded drift and below the chosen test load. Validate occupied/empty transition over 100 cycles. ToF direction counting needs site-specific installation calibration and must not be used for emergency occupancy.
7. Test bus disconnection, duplicate sequence, stale readings, broker outage and hub restart before pilot.

## Privacy, safety, and limitations

No personal identifiers, images, audio, device identifiers or per-person movement histories are collected. Raw seat states remain local; cloud sync should retain only aggregate 15-minute counts for at most 30 days and provide deletion. Post notices and consult the library’s privacy officer before deployment. Occupancy can be wrong due to bags, accessibility needs, sensor drift, or people sharing seats; visitors must verify availability in person. Never use this as an emergency headcount, access-control, or evacuation system. Prototype only; no field, EMC, radio, regulatory, accessibility or usability validation has occurred.

## Verification status

| Area | Status |
|---|---|
| Shared frame encode/decode/CRC | Host-compiled and tested by included round-trip test |
| Per-node C firmware | Illustrative SDK-independent state examples; not target-built |
| API | Python syntax and unit tests run if dependencies available; local demo only |
| ML | Synthetic-data smoke run; no real-data validation |
| Mobile | React Native source stub; native build not performed |
| Hardware | Netlists and BOMs are conceptual; no KiCad capture, ERC, PCB or physical validation |
| Security | No device authentication/encryption or production user auth; isolated prototype network only |

Invented and maintained by [jayis1](https://github.com/jayis1).
