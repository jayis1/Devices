# AEDReady — community AED cabinet readiness network

Reference design, unverified on hardware. This system helps a facility coordinator notice a cabinet opened, a cabinet environment outside a configurable storage band, a missing/overdue staff inspection, or a disconnected monitor. It does not certify an AED is present, functional, accessible, or safe to use. It is not an AED, medical device, emergency-dispatch system, or substitute for the manufacturer's instructions, local inspection policy, or emergency services.

## User, problem, and value

Primary user: a community-center or library facilities coordinator, during the morning opening walk. AEDs can become unavailable between scheduled manual checks; the coordinator needs a concise list of cabinets requiring a physical check rather than a claim that a device is ready. Intended outcome: reduce time spent checking cabinets that have no reported change and shorten the delay between a detectable cabinet event and staff awareness. No measured savings or safety benefit is claimed.

Different from FireSync (fire detection/response) and CivicCharge (outlet availability): AEDReady monitors access and self-reported inspection workflow for emergency equipment cabinets; it neither detects emergencies nor actuates equipment. Unlike commercial AED cabinets/connected services, this proposal is a local-first multi-cabinet status layer and makes no AED diagnostic or readiness guarantee.

Multi-node is necessary because cabinets are distributed, while the coordinator needs one facility view; the battery-backed sensors cannot be replaced by a phone-only checklist. Hub caches events locally and continues local indicators during internet outage. Cabinet nodes do not identify people. A door event is not proof of use; staff verify in person.

## System overview

```text
[CAB-01..N cabinet sentinel] -- BLE advertisements --> [Pi site gateway + local API]
[staff inspection beacon] ---- BLE/button ---------->          | SQLite / rules
                                                              +-- optional MQTT/TLS --> facility server
                                                              +-- local staff dashboard / RN scaffold
```

| Node | Parts / role | Power and links |
|---|---|---|
| Site gateway | Raspberry Pi Zero 2 W; BLE scan, SQLite event journal, local FastAPI and status dashboard | Listed USB supply; BLE, optional Wi-Fi Ethernet uplink |
| Cabinet sentinel ×N | ESP32-C3-MINI-1, normally-closed magnetic reed contact, TMP117 temperature sensor, status LED and test button | 2×AA alkaline through 3.3 V regulator; BLE advertisements |
| Inspection beacon | ESP32-C3-MINI-1, keyed staff button, status LED/buzzer; staff records cabinet inspection by selecting cabinet ID in local UI | USB 5 V supply; BLE uplink |

The inspection beacon is a physical local feedback station; authorized staff use the local web page to associate a check with a cabinet. No visitor interaction is required. The gateway labels a cabinet “check required” on stale telemetry, temperature excursion, door event, or overdue staff check. These are workflow prompts only.

## Cabinet sentinel electrical contract

Proposed module-level pin map (confirm against the exact ESP32-C3-MINI-1 module/board revision before PCB layout). GPIO numbers are not a finished schematic. All signals are 3.3 V; do not connect this device to AED circuitry.

| Signal | GPIO | Notes |
|---|---:|---|
| I²C SDA / SCL | 4 / 5 | TMP117 at 0x48; 3.3 V pull-ups |
| Door reed | 6 | External pull-up; closed-loop contact; debounce |
| User test button | 7 | Active low, pull-up |
| Status LED | 3 | Series resistor; active high |
| Battery monitor | 0 | Divider only; verify ADC range and calibration |
| Programming | USB/UART per development board | Keep accessible; no production module PCB specified |

Gateway is powered by a listed 5 V supply. Sentinel power budget is a design target, not measured: assume 2×AA 2000 mAh, 15 µA deep sleep, 20 mA radio/sensor wake for 20 ms every 60 s; ideal average ≈21.7 µA and ideal life ≈10.5 years. Real life is lower due to self-discharge, regulator quiescent current, temperature, retries, and battery pulse impedance; use 12 months as a conservative prototype service interval pending bench measurement. Never use this estimate to define AED storage conditions.

## Protocol

`firmware/common/protocol.h` defines the 24-byte version-1 little-endian fixed frame: version u8, kind u8, node_id u16, sequence u32, uptime_s u32, temperature_centi_c i16, battery_mv u16, flags u16, CRC16-CCITT u16. CRC detects accidental corruption only; BLE link encryption/bonding and authenticated enrollment are not implemented. Gateway rejects bad CRC, unsupported version, unknown kind, and repeated/non-increasing sequence. No remote commands are defined. See `docs/protocol.md`.

## Software and ML

- `software/dashboard/`: FastAPI local status service with an MQTT topic contract; MQTT publishing is intentionally an adapter stub, not a live broker integration. SQLite/event persistence is not implemented in this reference slice.
- `software/ml-pipeline/`: deterministic synthetic event classifier demonstration. Rules are authoritative; synthetic scores are not evidence of field accuracy or readiness prediction.
- `software/mobile-app/`: React Native display scaffold; not a production app or notification channel.
- No cloud account is required for local use. No PII, audio, imagery, or patient data is collected. Optional outbound MQTT would need TLS certificate validation, per-device identity and facility access controls before deployment.

## BOM estimates

| Assembly | Estimated USD | Scope |
|---|---:|---|
| Gateway | 40 | Pi Zero 2 W, listed supply, storage, enclosure |
| Cabinet sentinel | 11.12 each | BOM-listed components; cells, enclosure, PCB and assembly excluded |
| Inspection beacon | 13.20 | BOM-listed components; enclosure and assembly excluded |
| Example site (gateway + 2 sentinels + beacon) | 75.44 | Arithmetic sum of listed CSV line items; excludes installation, enclosures/PCB not listed, staff time, AED, taxes |

Per-node CSVs are in `hardware/bom/`; prices are rough planning estimates and require supplier verification. BOMs describe a prototype electronics set, not certified equipment.

## Assembly, test, and operations

1. Build and test only low-voltage boards on a bench. Keep the sentinel physically separate from AED electronics and the AED's manufacturer-approved enclosure.
2. Provision a unique node ID and cabinet label locally; verify door open/close events, temperature sensor plausibility, battery report, and gateway offline indication.
3. Staff inspect the AED exactly as required by its manufacturer and local policy; record that inspection at the local interface. An electronic acknowledgement does not replace physical inspection.
4. Verify alerts using a test cabinet, not by opening a live emergency cabinet unnecessarily. Confirm an internet outage does not suppress local status.
5. Change temperature alert bands only to the AED manufacturer's stated storage limits; no default threshold is prescribed here.

## Safety, privacy, and limitations

A false “normal” status could delay recognition of an unavailable AED; therefore the UI must use “no recent issue reported” rather than “ready,” and stale nodes are visibly unknown. A false alarm causes staff work; staff must physically verify. No actuator can lock, open, power, or modify the cabinet or AED. Retain only cabinet event/inspection data, locally, with facility-defined retention and access control. Avoid placing identifying staff details in event payloads.

No validated KiCad schematics, PCB, enclosure, firmware SDK integration, BLE security, regulatory review, RF test, long-term battery test, environmental test, or field trial is provided. Netlists are conceptual. The backend and app are prototypes. This repository is not a deployment guide for emergency response. Site owner must confirm applicable radio rules and product/electrical requirements; use certified power supplies, protect batteries from shorts, and do not modify or obstruct the AED enclosure.