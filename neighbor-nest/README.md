# NeighborNest

**Local-first shared indoor plant-care coordination for apartment common rooms and community centers.** A small system for volunteer caretakers who inherit a collection of unlabeled plants and inconsistent watering habits.

## User and outcome

Persona: Sam, a volunteer at a neighborhood center, opens the room on Monday and cannot tell which planters were watered over the weekend. Paper notes go missing; watering every pot on a fixed schedule causes both dry soil and root rot. MVP outcome: surface which pots are outside a caretaker-defined moisture band and record a human-confirmed visit. It does not dispense water or diagnose plant disease.

Multi-node necessity: planters are spread across rooms and near windows; low-power probes sit in the soil while a mains-powered gateway remains near the network. A separate check-in dock makes volunteer confirmation accessible without requiring a phone. Target prototype BOM: about US$95 before enclosure, tax and shipping; estimates are indicative, not quotes.

Distinctness: unlike UrbanHarvest/GreenPulse, NeighborNest does not grow crops, identify species, scan leaves, or actuate irrigation. It coordinates human care for shared indoor collections. Rejected: automated plant watering (already represented); plant disease scanner (GreenPulse); home garden scheduling (UrbanHarvest); occupant-presence-based reminders (privacy-invasive and unnecessary).

## System

Three cooperating nodes: a Wi-Fi hub, up to four soil probes, and a button/display check-in dock. Probe and dock use BLE advertisements to the hub; the mobile app is an optional local-network view. The hub queues observations locally and works without internet. No microphones, cameras, identity accounts, or cloud requirement.

```text
[Soil Probe ×1..4] -- BLE, measurement + battery --> [Hub / local API]
[Check-in Dock] ---- BLE, button-confirmed visit --->       |-- MQTT (optional TLS) --> server
                                                         |-- React Native local dashboard
                                                         `-- local SQLite spool
```

### Nodes and interfaces

| Node | SoC / radio | Sensors and I/O | Power |
|---|---|---|---|
| Hub | ESP32-C6-MINI-1; Wi-Fi + BLE 5 | I2C SSD1306 OLED, GPIO button, USB serial | 5 V USB-C; no battery |
| Soil probe | nRF52840 module; BLE 5 | SHTC3 temperature/RH on I2C; capacitive soil probe to SAADC | 2×AA alkaline, switched sensor rail |
| Check-in dock | nRF52840 module; BLE 5 | 3 buttons, SSD1306 I2C OLED, status LED | 2×AA alkaline |

Prototype pin mapping: hub I2C SDA GPIO6/SCL GPIO7, status LED GPIO8, button GPIO9; probe I2C SDA P0.26/SCL P0.27, capacitive sensor AIN0/P0.02, switched sensor enable P0.13; dock I2C SDA P0.26/SCL P0.27, buttons P0.11/P0.12/P0.14. Confirm module board pin mapping and regulator limits before PCB design. Schematic directory contains wiring/netlist specifications, not KiCad capture or production-ready layouts.

Power estimates are design targets, not measurements: probe sleeps at 8 µA, samples for 40 ms at 5 mA every 30 min; nominal 2000 mAh AA pair should exceed one year after self-discharge and regulator losses (validate experimentally). Dock sleeps at 15 µA and wakes on button; same pack target >1 year. Hub operates continuously from a certified isolated 5 V USB supply. Remove cells before servicing; no charging circuit is included.

## Protocol and failure behavior

`firmware/common/protocol.h` defines a bounded, versioned frame with node ID, monotonic sequence, event type, payload length and CRC-16/CCITT. BLE is transport, not authentication: initial pairing requires physical hub-button confirmation; deployments must use per-node BLE bonding and reject unpaired IDs. Replay/duplicate sequence numbers are ignored within a boot session; sequence resets require re-pairing. No remote actuation exists. Measurements include units and calibration metadata; stale values are shown as stale, never as current. Hub buffers locally; Wi-Fi/cloud loss cannot prevent local display or check-ins. Low battery and missing probes are explicit states.

## Software

- `software/dashboard/backend`: FastAPI demo telemetry and health endpoints with in-memory storage. MQTT integration, durable storage, and authentication are not implemented. Never expose this demo publicly; production requires persistent storage, TLS, and authenticated authorization.
- `software/ml-pipeline/train.py`: runnable synthetic-data demonstration of moisture trend classification. Synthetic output is not a plant-care model; production baseline is caretaker-defined threshold plus elapsed time.
- `software/mobile-app`: React Native concept screen with mock data; not connected to BLE or backend.
- `scripts/calibrate.py`: records dry/wet ADC endpoints as a local JSON calibration; repeat for each probe/soil mix.

## Validation and limitations

The electronics are a prototype proposal. No KiCad schematic capture, PCB, radio range test, plant trial, or battery-life measurement is supplied. Soil moisture is soil-specific and must not be interpreted as a universal watering prescription. ML is optional and must not override the human-defined thresholds. No automatic watering, account tracking, or personal data collection. BOM pricing and availability vary; verify exact module variants and electrical limits against current manufacturer documentation before ordering.

## BOM summary

Indicative listed-part subtotal for one hub, two probes and one dock: about US$47; allow roughly US$19 for enclosure/interconnect prototypes (about US$66 total). Unit costs in CSVs are rough estimates and exclude shipping, tax, PCB fabrication, and assembly.

## Build and run

Backend: `python -m pip install -r software/dashboard/backend/requirements.txt`; `uvicorn app:app --app-dir software/dashboard/backend --host 127.0.0.1 --port 8000`. Train demo: `python software/ml-pipeline/train.py`. Mobile package is a UI mock; install using the package manager appropriate to a React Native environment. Firmware targets Zephyr and requires west plus an nRF52840 board definition; hub requires ESP-IDF. Hardware is not certified or validated for deployment.

Authorship: jayis1. Prototype design status; not a validated product.