# SpinShare — shared laundry, without the wasted trips

SpinShare is a local-first monitoring kit for apartment laundry rooms and small laundromats. Residents can check whether a washer or dryer is likely free before carrying a load downstairs; operators get fault evidence instead of relying on vague reports. It measures machine vibration and door state only: no camera, microphone, payment integration, or control of mains equipment.

A resident in a building with a basement laundry room currently makes repeated trips to check machines. SpinShare aims to replace one avoidable check trip per laundry session (an estimate, not a measured result). Unlike consumer washer apps tied to a single appliance brand, it retrofits shared mixed-brand equipment using external sensors and gives an operator-controlled, building-local status board. It does not reserve machines or identify residents.

## System and node rationale

```
[Washer vibration + door node] --BLE--> [Room gateway / status API] <--BLE-- [Dryer vibration + door node]
                                                  | Wi-Fi / MQTT (optional)
                                                  +--> React Native client
```

Two sensor nodes are required because machines are physically separated and each needs its own vibration and door observations; the gateway aggregates state and remains useful without cloud access. Washer/dryer classification is a configurable installation label, not inferred from user behavior. No actuators or safety interlocks are provided.

| Node | Core parts | Purpose |
|---|---|---|
| Room gateway | Raspberry Pi Zero 2 W, USB-C 5 V supply | BLE collection, local status cache and HTTP API |
| Machine sensor ×2 | nRF52840 module (Ebyte E73-2G4M08S1C), LIS3DH accelerometer, door reed switch | Detect vibration cycles and door-open evidence; BLE telemetry |

Parts and per-node illustrative cost estimates are in `hardware/bom/`. Estimates exclude enclosure, labor, tax, and certification. Target kit retail: $80–150 per room, subject to pilot BOM and installation costs.

## Data flow and states

Sensors sample acceleration at 25 Hz during a machine cycle and report a 10-second RMS vibration window, door state, battery estimate, sequence and uptime. Gateway classifies states using explicit hysteresis: `running` after sustained vibration, `possibly_done` after vibration falls for a configurable 3 minutes, `available` only after vibration is quiet and door-open evidence is seen, otherwise `idle_unknown`. Missing/stale (>90 s) reports are `offline`, never treated as free. The UI labels uncertain states. A person must verify before use.

BLE GATT uses a versioned compact telemetry payload; see `docs/protocol.md`. The gateway is authoritative for aggregation. Optional MQTT publishes only coarse state (`spinshare/v1/<site>/<node>/telemetry`); no remote commands exist. On internet loss, local BLE collection and the local HTTP API continue. The demo API uses an in-memory cache and loses history on restart.

## Setup

1. Assemble low-voltage sensor boards from the pin/net references in `schematic/` and BOMs; attach the reed sensor to an existing door edge without modifying the appliance.
2. Build sensor firmware with Zephyr SDK (nRF Connect SDK 2.6 or compatible Zephyr 3.x), set a unique node ID and BLE pairing policy, and flash using the board's SWD pads. Gateway firmware is a Python service on Raspberry Pi OS.
3. Install backend: `python -m pip install -r software/dashboard/requirements.txt`; run `uvicorn main:app --app-dir software/dashboard --host 127.0.0.1`.
4. Open the React Native scaffold with Expo (`npm install`, `npx expo start`) and set the gateway URL in `App.tsx`.
5. Calibrate with an empty, safe test cycle; thresholds and installation notes are in `scripts/calibrate.py`.

## Privacy, safety, and limitations

No personal identity, household occupancy, audio, image, or payment data is collected. Site operators control retention; default gateway cache is RAM-only. BLE identifiers must be randomized or provisioned per installation. MQTT is opt-in and should use TLS, per-site credentials, and a private broker; the example has no production authentication. This is an availability advisory, not a machine-control or safety system. Do not open appliances, connect to mains wiring, defeat guards, or rely on a status as proof a machine is safe. Installation must not obstruct doors, vents, or emergency access.

## Validation / maturity

Concept and reference implementation. The BLE hardware, signal thresholds, RF range, battery life, and component pricing are not physically validated. The netlists are pin-reference documents, not KiCad schematics. Firmware is a Zephyr-oriented example and is not claimed as compiled or field-tested. Backend and ML code are demonstration scaffolds; the synthetic-data model is not a validated predictor. No electrical, radio, or product certification has been obtained. No real-world time-saved or accuracy claims are established.

## Repository map

- `firmware/`: shared payload contract and sensor-state example.
- `schematic/`: per-node connection references.
- `software/dashboard/`: local FastAPI status endpoint and optional MQTT publishing contract.
- `software/ml-pipeline/`: reproducible synthetic cycle classifier demonstration; thresholds remain the safe v1 fallback.
- `software/mobile-app/`: React Native status screen scaffold.
- `docs/`: architecture, protocol, API and power notes.
- `scripts/`: threshold calibration helper.

Created and maintained by jayis1.
