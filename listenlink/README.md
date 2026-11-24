# ListenLink — venue assistive-listening readiness

ListenLink helps community theaters, lecture rooms, and public meeting venues keep assistive-listening equipment discoverable and ready. It reports operational readiness and staff-entered inventory only; it does not transmit audio, identify listeners, or claim hearing or accessibility compliance. All radio operation is disabled in this reference build pending venue and jurisdiction review.

## System and boundaries

Three cooperating units: Venue Hub (ESP32-C6, local Wi-Fi/BLE commissioning and encrypted MQTT uplink), Receiver Dock (ESP32-C3, 8 low-voltage receiver-bay presence switches), and Room Beacon (nRF52840, staff-operated transmitter-on/ready button and battery-powered BLE status). The hub joins the room status and dock inventory, keeps a local last-known state during internet loss, and exposes a FastAPI API. This is operational telemetry, not an audio relay; venue audio systems remain untouched. The hub is required to reconcile room readiness with the actual number of loan receivers and to buffer events offline.

Radio: BLE 5 advertisements from room beacon to hub; Wi-Fi/TLS MQTT from hub to broker. The dock is wired I²C GPIO expander to its ESP32-C3, USB 5 V powered. No custom Sub-GHz transmitter or unlicensed audio broadcast. BLE advertisement payloads are bounded, versioned, and contain no personal data. Prototype identity keys must be provisioned uniquely by the venue; no shared production key or credentials are included.

### Readiness contract

A room is `ready` only when the staff button says its existing venue transmitter is on and at least one receiver is reported present in the dock. `unknown` is used for stale room telemetry (>120 s), invalid versions, missing nodes, or hub restart before reconciliation. A dock switch means only physical bay presence, not charge, function, hygiene, or successful checkout. Staff must test equipment and provide assistance. No automatic control of transmitters, receivers, doors, or venue systems.

Packet v1 is defined in `docs/protocol.md`: 13 bytes with CRC-8/ATM. Reject wrong length, version, magic, CRC, and non-increasing sequence; sequence wrap requires re-pairing. BLE advertisement payloads must be encrypted/rotating-address in a production implementation; this minimal example deliberately does not claim that security has been implemented. MQTT uses TLS and broker ACLs configured by the operator; API is local-development only and must not be exposed publicly without authentication.

## Hardware / build

See `docs/architecture.md` and per-node `hardware/bom/*.csv`. `schematic/` contains pin/net reference sheets, not KiCad ERC-verified drawings. Pin plans are reference assignments; validate against chosen module revisions before PCB fabrication. All electronics are SELV, USB-powered or battery-powered; no mains connection. Use certified off-the-shelf venue audio equipment and retain its controls.

Firmware shared protocol tests: `cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/listenlink-test && /tmp/listenlink-test`. Node programs are portable C reference application logic; board SDK peripheral/radio integration is not included. Backend: `python -m pip install -r software/dashboard/requirements.txt`, then `uvicorn main:app --app-dir software/dashboard --reload`; run `pytest -q software/dashboard`. MQTT is optional for local demonstration and requires operator broker configuration. ML contract and baseline are in `software/ml-pipeline/`; no trained model or performance claim. Mobile is a React Native display stub and has not been built on a device.

## Privacy, maintenance and limitations

No audio, user identity, or listener activity is collected. Retain aggregate readiness events only as needed by the venue; default demo keeps in-memory state and loses it on restart. Staff can always inspect and test equipment manually. Replace the button-cell beacon battery when low; inspect receiver bays and switches monthly. Prototype only: no certification, accessibility compliance determination, safety guarantee, or production security review. Validate BLE range, battery life, switch mechanics, RF coexistence, electrical design and venue workflows before use.

Authored by jayis1.
