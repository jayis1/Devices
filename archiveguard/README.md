# ArchiveGuard

ArchiveGuard is a local-first environmental observability system for small community archives, museums, and volunteer-run collections. It helps staff notice unsuitable storage conditions and document inspections; it does not certify preservation conditions or control HVAC/dehumidification equipment.

## Readiness

Design prototype, not validated hardware. Firmware and backend vertical slice are source artifacts; no PCB, enclosure, calibrated sensor, RF, or collection-preservation validation has been performed. Component pinouts and electrical limits must be checked against the selected manufacturer revisions before fabrication.

## System and use

Three cooperating node roles: an ESP32-C6 gateway with Wi-Fi and 2.4 GHz 802.15.4, one or more battery-capable room sentinels, and a removable shelf-probe node. Nodes send temperature, relative humidity, and (shelf probe only) surface temperature via encrypted Zigbee application transport to the gateway. The gateway bridges measurements to MQTT over TLS when configured. Local buffering and local dashboard ingestion continue without cloud access. All output is advisory and staff inspection remains authoritative.

Data path: SHT31 room sensor / TMP117 shelf contact probe → ESP32-C6 node → Zigbee → gateway → MQTT topic `archiveguard/v1/sites/{site_id}/telemetry` → FastAPI validation/store → mobile client API. This prototype transport adapter is a documented interface; the included host simulator exercises JSON telemetry, not radio hardware.

## Nodes and candidate components

| Node | SoC / parts | Purpose / power |
|---|---|---|
| Gateway | ESP32-C6-WROOM-1, USB-C 5 V input, 3.3 V buck (TPS62162 candidate) | Wi-Fi/MQTT bridge; USB powered |
| Room Sentinel | ESP32-C6-MINI-1, Sensirion SHT31-DIS, 3.3 V LDO (MCP1700 candidate), protected 18650 holder | Air temperature/RH; prototype battery-powered |
| Shelf Probe | ESP32-C6-MINI-1, TMP117, 3.3 V LDO, protected 18650 holder | Contact temperature and local air temperature/RH if populated with SHT31 |

Parts are candidates, not a released BOM. Module antenna keep-out, regulator peak loads, charging/protection, connector pinouts, and footprints require datasheet/layout review. Do not charge cells in the prototype; use protected cells and an external approved charger. No mains wiring or actuators are included.

## Interfaces and design

- Radio: Zigbee over 802.15.4, gateway is coordinator; commissioning must use a site-specific install code and unique keys. The host demo omits radio and key provisioning.
- Sensor bus: I²C, 3.3 V, SDA/SCL with 4.7 kΩ pull-ups; address conflicts are avoided by separate node buses.
- Core sample fields: schema_version=1, site_id, node_id, seq, captured_at (UTC RFC3339), temperature_c, relative_humidity_pct; optional surface_temperature_c. Range-check values and reject unknown schema versions.
- Node IDs are provisioned identifiers, not secrets. Radio and MQTT credentials must be unique per installation, never committed. MQTT TLS and per-topic ACLs are required in deployment.
- Link failure: sample locally, bounded 128-record ring buffer; retry with capped exponential backoff. Gateway/cloud failure does not change collection conditions; staff use independent calibrated instruments and normal conservation procedures.
- Security: use Zigbee network/application encryption, authenticated commissioning, TLS MQTT, rotate/revoke site credentials. OTA signing and secure boot are production requirements not implemented here.

## Power and safety

Design target 5 V USB gateway; sensor nodes run from protected 1-cell Li-ion through 3.3 V regulation. Validate radio transmit peaks, regulator dropout, brownout behavior, enclosure temperature, cell protection and runtime before use. Low battery and stale data are surfaced; no threshold is represented as universally safe. Keep probes electrically isolated from collection materials and use non-damaging removable mounts.

## Software

- `firmware/common/protocol.h`: bounded C telemetry structure and validation.
- `firmware/host_sim/`: portable C simulator for protocol validation.
- `software/dashboard/`: FastAPI ingestion/read API, local SQLite prototype.
- `software/ml-pipeline/`: deterministic baseline anomaly flags; no trained model or field dataset is supplied.
- `software/mobile-app/`: React Native interface contract stub; not a complete application.
- `schematic/`: conceptual node connection sheets; not KiCad manufacturing schematics.
- `hardware/bom/`: candidate BOMs; reconcile against final CAD before ordering.

Run backend: `cd software/dashboard && python -m pip install -r requirements.txt && uvicorn app:app --host 127.0.0.1 --port 8000`. Run tests: `python -m unittest discover -s software/dashboard/tests`. Run C protocol test: `cc -std=c11 -Wall -Wextra -Werror firmware/host_sim/main.c -o /tmp/archiveguard-check && /tmp/archiveguard-check`.

## Privacy and limitations

No audio, images, occupant identity, or personal data are collected. Prototype stores telemetry locally in SQLite without retention automation; operators should restrict filesystem access and delete data according to their site policy. ML flags are descriptive only. No preservation, safety, regulatory, or sensor-accuracy claim is made.

## Validation before deployment

Review datasheets and schematic/ERC, assemble and calibrate each probe against traceable instruments, characterize battery life and radio coverage, test clock drift/reboot/buffer-full behavior, deploy authenticated MQTT with ACLs, establish data retention, and obtain a conservator's approval for alert interpretation.
