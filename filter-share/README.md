# FilterShare — community air-cleaner lending, without tracking households

Status: engineering prototype concept. FilterShare helps a library, mutual-aid group, or school lend portable HEPA cleaners during wildfire smoke and local pollution events. A checkout dock records device presence and filter service status; a room sentinel measures indoor PM2.5 at the lending site; a gateway publishes anonymous stock and equipment condition. It does not monitor borrowers, diagnose health, certify clean air, or control mains appliances.

A volunteer can answer “is a loaner available, and is its filter service due?” without calling several sites. Estimated benefit is fewer unsuccessful trips; no outcome study has been performed.

## Novelty and choices

The README inventory already covers indoor exposure (BreathHome), community cooling (CoolStop), and shared tools (ToolShare). FilterShare is a lending-fleet and filter-service coordination system, not an occupant exposure tracker or room comfort service. Alternatives include ordinary library catalogs and commercial air monitors; neither provides this combination of anonymous equipment presence, filter service records, and local particulate context. The inventory has no comparable lending workflow. Candidate alternatives rejected: another household air monitor (duplicate BreathHome); a wildfire alert wearable (health/location/privacy burden).

## Architecture

```
PM sentinel (ESP32-C3 + SPS30) -- ESP-NOW --> Site gateway (ESP32-S3)
Loan dock (ESP32-C3 + reed switches + buttons) -- ESP-NOW --> gateway
Gateway -- Wi-Fi/MQTT over TLS --> optional FastAPI service --> React Native volunteer view
```

Nodes: one gateway, one particulate sentinel, one or more loan docks. Local display/status remains available at the site if internet fails. No actuators or mains wiring. ESP-NOW is a prototype site-local transport, not a certified mesh; all devices require provisioned unique peer keys before deployment. Cloud MQTT is optional and must use TLS and per-gateway credentials.

### Hardware and pin map

Prototype uses 3.3 V ESP32-C3-MINI-1 modules. Sentinel: Sensirion SPS30 (5 V supply, UART 3.3 V logic) on GPIO20 RX / GPIO21 TX; status LED GPIO8 through 1 kΩ; UART programming via USB-UART bridge, EN/BOOT exposed. Dock: reed contacts on GPIO3 and GPIO4 with internal pull-ups; confirm and service buttons GPIO5 and GPIO6; active-low LED GPIO7 through 1 kΩ. Gateway: ESP32-S3-WROOM-1, USB-C 5 V input, Wi-Fi uplink, ESP-NOW radio, status LED GPIO2 through 1 kΩ. Firmware files contain the matching assignments. Avoid strapping pins for external loads; validate final module variant and board pinout before PCB layout.

Power: certified isolated 5 V USB supply, local 3.3 V regulator sized for radio bursts; sentinel 5 V rail powers SPS30. Battery operation is intentionally out of scope. No battery charging or mains interface. Do not place the particulate inlet where it is blocked by the enclosure.

## Protocol and data

Shared compact frame: little-endian `magic:u16, version:u8, type:u8, node_id:u32, seq:u32, uptime_ms:u32, value_a:i32, value_b:i32, crc16:u16` (26 bytes). CRC-16/CCITT-FALSE covers preceding bytes. PM2.5 is integer micrograms/m³ ×10; dock `value_a` is a bitmask of present loaners and `value_b` a bitmask of service-due units. Sequence numbers reject duplicates within a boot; uptime and gateway receipt time support freshness without trusting node wall clocks. See `docs/protocol.md`.

## Safety, privacy, degraded operation

Particulate values are contextual observations, not calibrated regulatory measurements or health advice. Do not use this prototype to determine whether a room is safe. Loaners remain manually inspected; a due flag does not prove filter condition. Do not connect or switch a borrower's appliance. No cameras, microphones, borrower identity, or location. The dock records equipment state only. Retain site-level telemetry for 30 days by default; disable cloud upload for local-only use.

| Failure | Behavior |
|---|---|
| Internet/API down | Gateway keeps latest status locally; dock LEDs continue; queued cloud events expire after 24 h |
| Sentinel missing/stale | Show “air reading unavailable”; never infer clean air |
| Dock or gateway lost | Staff use manual paper inventory; no automatic availability claim |
| Invalid/duplicate frame | Drop, count locally, bounded retry with jitter |

## BOM estimates

See `hardware/bom/*.csv`; indicative prototype component costs: gateway $18, sentinel $65 (SPS30 dominates), dock $12, excluding enclosure, assembly, calibration, and tax. Prices are estimates, not quotes. Use manufacturer datasheets for final design; verify component availability and regional radio compliance.

## Software and build

Firmware is portable C showing packet framing and node state, not a complete ESP-IDF application. Schematics are honest netlist/pin specifications in `schematic/`, not KiCad capture or ERC-checked files. Backend is a FastAPI API stub and requires deployment hardening (identity, authorization, TLS broker ACLs, durable queue) before real use. Synthetic ML baseline is optional and advisory only; it is not necessary for inventory and has no validated predictive performance. React Native app is a screen/API stub.

Run `python3 -m pytest -q software/dashboard/tests` and `python3 software/ml-pipeline/train.py` from this directory. The backend stores only in-memory snapshots. Physical radio interoperability, electrical design, EMC, air-sensor calibration, security, app build, and field usability remain unverified. MIT, consistent with repository. Invented and maintained by jayis1.
