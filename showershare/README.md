# ShowerShare

ShowerShare is a local-first readiness monitor for community and shared-use shower facilities. It helps an operator distinguish an available stall from one awaiting a human cleaning or maintenance check, without cameras, microphones, identity tracking, reservations, or autonomous plumbing control.

Readiness: software prototype and concept-level electronics; no PCB, enclosure, wet-location, RF, or safety validation. Not a hygiene certification or substitute for inspection. Never connect to mains or plumbing controls.

## System and operating boundary

Three cooperating units: the ESP32-C6 gateway coordinates two ESP32-C3 low-voltage sensor nodes over encrypted Wi-Fi; one Stall Sentinel per stall observes door state (reed switch) and floor presence (load cell via HX711, coarse threshold only); the Water Sentinel observes pipe surface temperature (DS18B20) and non-invasive clamp-on flow pulses (hall sensor). The gateway buffers events locally and exposes an authenticated FastAPI/MQTT interface when configured. Mobile app is a prototype status view. Manual staff inspection determines clean/ready status; sensors only report context.

Nodes use 5 V USB supplies with 3.3 V logic; keep all electronics outside wet zones in suitable enclosures. Flow sensing requires an isolated clamp-on meter; no water-contact installation is specified. Door and floor signals are privacy-preserving but must not be used to infer individual behavior. Offline operation: nodes retain latest readings, gateway queues bounded events; loss of connectivity never changes facility operation.

## Interfaces

Stall GPIO4 reed input (active-low, 10 kΩ pull-up), HX711 DOUT GPIO5 and SCK GPIO6. Water Sentinel DS18B20 one-wire GPIO4 (4.7 kΩ pull-up), pulse input GPIO5 (10 kΩ pull-up). Gateway receives HTTPS/MQTT over Wi-Fi. MQTT topics: `showershare/v1/{site}/{node}/telemetry`; payload fields `v,node,seq,ts,kind,value,unit`. Never publish personal or occupancy histories; aggregate and expire raw observations after 24 h by default. TLS and broker ACLs are required in deployment; development mode binds loopback only.

## Hardware and BOM

See `hardware/bom/*.csv`; pin tables and block diagrams in `docs/architecture.md`. Schematics in `schematic/` are design notes, not validated KiCad projects. Components are representative; verify exact module variants, enclosure suitability, isolation and local electrical codes before construction.

## Software

- `firmware/`: portable C acquisition/protocol examples; hardware drivers and radio integration remain board-specific.
- `software/dashboard/`: FastAPI prototype with MQTT-compatible event ingestion boundary.
- `software/ml-pipeline/`: deterministic synthetic-data baseline only; no real-world model or performance claims.
- `software/mobile-app/`: React Native screen stub.
- `scripts/`: safe local test and calibration guidance.

## Build and checks

Run `python3 -m unittest discover -s software/dashboard -v` from this directory. Compile portable protocol examples with `gcc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/stall-sentinel/main.c -o /tmp/showershare-stall` and similarly for `water-sentinel`. These examples do not include ESP-IDF drivers, radio security, or production provisioning. Do not deploy without authentication, TLS, broker ACLs, and a privacy review.
