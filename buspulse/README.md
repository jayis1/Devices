# BusPulse

Status: prototype reference design; not certified, field-tested, or a source of official transit arrival times. Author: jayis1.

BusPulse helps a small transit agency or volunteer-run rural stop identify whether its passenger-facing arrival display is working and whether its solar/battery supply is degrading. The operator opening a rural route can see a dead display before riders wait in uncertainty. A pilot can measure the rate of unreported display outages; no savings or accuracy is claimed. It does not estimate arrivals or track passengers.

Why multiple nodes: a roadside display observer and separately mounted power/weather sentinel provide independent failure evidence; a gateway relays observations without cellular hardware at each stop. Unlike commercial real-time passenger signs, this is low-cost maintenance telemetry for sites without AVL integration—not a passenger information product.

## Architecture

```text
Display Observer (ESP32-C3 + light/voltage monitor) --LoRa-->
Power/Weather Node (STM32L072 + voltage/temp) --------LoRa--> Site Gateway (ESP32-S3)
                                                                  | Wi-Fi / local API
                                                                  v
                                                          FastAPI service + app stub
```

Three hardware nodes: gateway plus two field nodes. Reference radio is 915 MHz SX1276 point-to-point, subject to local frequency, EIRP, antenna, and duty-cycle rules. Gateway buffers status during internet outages. Missing packets and failed sensors mean unknown/stale, never healthy. No camera, microphone, passenger counter, or personal data. No control of traffic, vehicles, or public infrastructure.

## Hardware and software

See `docs/design.md`, `docs/protocol.md` and per-node CSV BOMs. `schematic/` contains text connection specifications, not KiCad schematics. Portable C codec is host-buildable; node transport/acquisition is an integration stub. FastAPI is a local prototype. MQTT is not enabled. ML is intentionally not used for core value. Mobile UI is a scaffold. No credentials are supplied; API is unauthenticated and must remain loopback/trusted-lab only.

## API and test

`software/dashboard/main.py` exposes `GET /health`, `POST /telemetry`, and `GET /nodes`. Start locally with `uvicorn main:app --app-dir software/dashboard --host 127.0.0.1`; run API tests from the repository root with `python3 -m pytest buspulse/software/dashboard/tests -q`. The service stores in memory and is not production-ready. No MQTT broker adapter is active.

## Safety, privacy, limits

Installation requires site-owner approval; do not attach to transit-control, signal, vehicle, or mains circuits. External sensing must be isolated and voltage-limited. Outdoor enclosure, lightning protection, pole loading, battery charging, weatherproofing, RF compliance, and accessibility review are unvalidated. Use an appropriately protected commercial solar/battery assembly. No passenger identity or occupancy inference. Operator confirmation is required before maintenance action.

## Verification levels

Specified: architecture, protocol, BOM and connection notes. Not target-built or hardware-tested: embedded nodes, RF range, power system, enclosure and mobile app. Costs are planning estimates only; verify exact variants and supplier availability before purchasing. This is a prototype, not a validated installation design.
