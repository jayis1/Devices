# AirShare

Status: experimental reference design. Electrical behavior, radio range, and physical installation are unvalidated; not a certified indoor-air or health device.

## Problem and distinction

Residents in multi-unit buildings often cannot tell whether a shared laundry room is stuffy during peak use or whether a ventilation complaint merits a maintenance visit. AirShare gives residents and building staff anonymous, room-level ventilation context and evidence for maintenance triage. Relief moment: a display says “Laundry room: ventilation weak; please report,” rather than asking residents to infer safety. Unlike BreathHome and other personal/home air systems, this monitors shared common rooms, does not track occupants, control HVAC, or diagnose health conditions.

## Architecture

```
Room Sentinel (CO2/RH/temp) -- BLE 5.0 --> Site Hub (ESP32-S3)
Doorway Context Node (door events only) -- BLE --> | Wi-Fi/MQTT/TLS
                                                   v
                                           FastAPI + SQLite
                                                   ^
                                           React Native client
```

Nodes are physically separate because room air and doorway context are distinct measurement locations; the gateway is installed by the building operator and aggregates without exposing personal data. No camera, microphone, occupancy count, or identity is collected. Hub loss makes readings stale; nodes advertise latest readings after reconnection. No actuator exists. Door-open state is contextual only, never proof of ventilation.

## Hardware and interfaces

Prototype reference choices: site hub ESP32-S3-DevKitC-1 (USB 5 V); room sentinel ESP32-C3 module (USB 5 V) with Sensirion SCD41 (I2C 0x62) and SHT31 (0x44); doorway context node nRF52840 module (3.3 V USB supply), reed switch to GPIO with pull-up. BLE 5.0 star; hub Wi-Fi to broker. Use only USB SELV supplies, no mains wiring. Exact pin map and component list are in `docs/design.md`; BOM estimates are in `hardware/bom/`. Schematics are pin/net design notes, not KiCad capture or fabrication files.

## Software

Portable shared C telemetry codec under `firmware/common`; concise node acquisition examples under `firmware/nodes`. FastAPI ingestion API accepts validated JSON and exposes read-only latest status; MQTT topic contract is `airshare/v1/{site_id}/{node_id}/telemetry`. ML directory provides a deterministic ventilation-context baseline, not a trained model or safety classifier. Mobile app is a minimal React Native starter that displays site status; it is not a production client.

## Privacy, security, and limitations

Only room CO2 ppm, temperature, RH, door open/closed, node ID, sequence, and timestamp are transmitted. No raw personal data. Building operator chooses retention and deletion; prototype SQLite is local. Configure per-device credentials, broker ACLs, and TLS before deployment; sample service is unauthenticated and must remain on loopback/trusted test LAN. Never expose it publicly. CO2 is a ventilation proxy, not a measure of infection risk or a substitute for code-compliant ventilation. Calibrate sensors and seek qualified building assessment for complaints.

## Bring-up

Install Python 3.11+, `pip install -r software/dashboard/requirements.txt`, then run `uvicorn app:app --app-dir software/dashboard --host 127.0.0.1 --port 8000`. Run tests with `python -m unittest discover -s software/dashboard/tests`. Portable protocol test: `cc -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/airshare-test && /tmp/airshare-test`. Hardware SDK integration, radio commissioning, KiCad ERC, mobile build, physical calibration, and security review remain outstanding.

Author: jayis1.
