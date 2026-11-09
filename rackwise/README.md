# RackWise — community bicycle rack availability

Status: implemented reference prototype; software checks only. Not bench-tested, not a certified occupancy or security system.

RackWise helps a person decide whether a public bicycle rack is likely to have space before they arrive. A site gateway collects coarse, anonymous rack occupancy from two or more low-voltage rack segments and combines it with local weather context. It reports availability bands, not bicycle identities, reservations, theft alerts, or guaranteed spaces. It addresses public bicycle parking rather than bicycle condition/repair (SpokeSense, CommunityRepair) or route safety (CycleGuard).

## First deployment

A small indoor or sheltered pilot uses one Raspberry Pi Zero 2 W gateway, two ESP32-C3 rack observers, and one ESP32-C3 weather/context node. Observers use non-imaging break-beam pairs across rack positions; no camera, microphone, radio tag, or per-bike identifier is collected. Nodes report occupied/clear/unknown per bay over Wi-Fi/MQTT on a private site network. Gateway aggregates to five-minute counts and exposes a read-only FastAPI endpoint; a React Native screen displays the last update and coarse band. ML script demonstrates reproducible baseline evaluation on explicitly synthetic data only; it makes no real demand forecast.

Deployment is advisory: sensors can be occluded, misaligned, or fooled by pedestrians. Staff should verify rack condition and provide a physical sign/contact route. No data is used to ticket, identify, reserve, or monitor individuals. Retain raw bay states locally for at most 24 hours; aggregate hourly counts for 30 days, then delete. Do not publish precise live counts where that could expose sensitive site use.

## Architecture and data flow

`break-beam pair -> ESP32-C3 observer -> MQTT (TLS on site broker) -> gateway aggregation -> FastAPI /v1/sites/{site}/availability -> mobile display`

The weather node reports temperature and precipitation context, but weather is informational and does not change occupancy. Hub is the coordinating authority; it marks stale nodes unknown after 3 missed 60-second reports and never silently treats missing as clear. MQTT credentials are deployment-provisioned and unique per device; sample configuration has no credentials. Production requires broker ACLs and TLS certificates. Internet loss does not interrupt local MQTT/API; restart loses up to the in-memory aggregation window. Persistent production storage is deferred.

## Nodes and hardware

| Node | SoC / sensors | Power and role |
|---|---|---|
| Gateway | Raspberry Pi Zero 2 W; Wi-Fi; local MQTT broker + API | Regulated 5 V USB supply; aggregates and serves local status |
| Rack observer ×2+ | ESP32-C3-MINI-1; two 3.3 V break-beam receiver inputs per bay; status LED | 5 V SELV adapter to 3.3 V regulator; reports debounced state, no actuation |
| Context node | ESP32-C3-MINI-1; BME280 I²C temperature/humidity/pressure | 5 V adapter/regulator; contextual telemetry only |

Prototype wiring diagrams and pin maps are in `schematic/`. They are explicit wiring references, not KiCad PCB schematics. No PCB layout, enclosure, antenna validation, EMC/regulatory evaluation, or production electrical safety review is supplied. Verify exact module variant and sensor electrical limits against vendor datasheets before assembly. Keep all field wiring low-voltage; do not connect to mains.

## Protocol

See `docs/protocol.md`. JSON v1 messages include `schema`, `device_id`, monotonic `seq`, UTC `observed_at`, and a typed payload. `bay_state` is `occupied|clear|unknown`; raw beam levels are not exposed. Messages are idempotent by `(device_id,seq)` and stale sequence numbers are rejected. MQTT transport uses TLS and broker per-device publish ACLs in deployment; JSON itself is not cryptographically authenticated. The prototype has no OTA implementation; firmware updates are local and physically supervised.

## Software and build

- `firmware/`: portable C protocol/state logic and node examples; hardware GPIO/MQTT adapter is an integration boundary, not a complete ESP-IDF target.
- `software/dashboard/`: FastAPI read-only prototype; accepts validated in-process events and provides health/status endpoints. MQTT subscriber integration is explicitly a deployment adapter TODO.
- `software/ml-pipeline/`: deterministic synthetic-data baseline, no real-world claims.
- `software/mobile-app/`: React Native UI stub; not a complete app or store-ready build.
- `hardware/bom/`: node-level parts and quantities.

Run checks from this directory:

```sh
python3 -m unittest discover -s software/dashboard/tests -v
python3 -m py_compile software/dashboard/main.py software/ml-pipeline/train.py
python3 software/ml-pipeline/train.py
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/tests/test_protocol.c -o /tmp/rackwise-protocol-test && /tmp/rackwise-protocol-test
```

No physical hardware, ESP-IDF build, broker integration, KiCad ERC, or React Native build has been validated. See `docs/architecture.md`, `docs/api.md`, and `docs/protocol.md` before adapting the prototype.