# PawStation — neighborhood dog-waste station readiness

Status: reference prototype specification, not field-tested or safety-certified. Invented and maintained by [jayis1](https://github.com/jayis1).

PawStation helps parks crews and neighborhood stewards keep public dog-waste stations stocked and serviceable. A caregiver walking a dog should not discover an empty bag dispenser or overflowing bin only after needing it. The system observes consumable level, bin fill, and station condition; it publishes maintenance advisories to a local dashboard without identifying people or animals. It does not count visitors, open bins, dispense bags, or make sanitation/safety claims.

## Architecture

```text
[Bag cassette node] --BLE--> [Site gateway] --Wi-Fi/MQTT--> [FastAPI + SQLite]
[Bin fill node] -----BLE--/                       |               |
[Inspection button] --BLE------------------------+          [React Native stub]
                                                   +-- local queue when offline
```

Spatially separate nodes are essential: dispensers and bins can be across a park, while the gateway can be mounted in a sheltered maintenance cabinet. The gateway caches events locally during internet loss. Sensor readings are coarse maintenance evidence; a human confirms replenishment and emptying.

### Nodes / reference pin assignments

| Node | SoC / components | Interface and pins (reference) | Power |
|---|---|---|---|
| Site gateway | Raspberry Pi Pico W (RP2040 + CYW43439), Wi-Fi; USB serial | UART0 TX GP0/RX GP1; I2C0 SDA GP4/SCL GP5; 3V3 logic | Certified 5 V USB supply |
| Bag cassette | ESP32-C3-MINI-1, VL53L0X ToF level sensor | I2C SDA GPIO4/SCL GPIO5; BLE 5; 3V3 | 2×AA alkaline; deep-sleep target, runtime unmeasured |
| Bin node | ESP32-C3-MINI-1, VL53L0X ToF level sensor, reed lid switch | I2C SDA GPIO4/SCL GPIO5; reed GPIO6 with pull-up; BLE 5 | 2×AA alkaline; protected enclosure |
| Inspection beacon | nRF52810 module, momentary service-confirm button, status LED | Button P0.11, LED P0.13; BLE advertising | CR2032 candidate; life unmeasured |

Pin assignments are illustrative module-level choices; review module datasheets and board-specific pin availability before layout. Nodes use BLE advertisements with application-level event IDs; gateway uses Wi-Fi only for upstream transport. No actuator or mains circuitry.

## Protocol

JSON event schema: `{version:1,node_id,seq,kind,value,unit,utc_ms}`. `kind` is `bag_level_pct`, `bin_fill_pct`, `lid_open`, or `service_confirmed`; percent is integer 0–100. Gateway deduplicates `(node_id,seq)`, rejects unknown versions and out-of-range values, and timestamps receipt if device time is unavailable. MQTT topic `pawstation/<site_id>/events`; broker credentials and TLS are deployment requirements, never defaults in this repository. BLE identity enrollment is manual via physical button; production cryptographic pairing and signed OTA remain unimplemented.

## Software and ML

`software/dashboard` provides a small FastAPI ingestion endpoint and an offline SQLite event log. MQTT is an optional adapter configured through environment variables. `software/ml-pipeline` contains a deterministic baseline generator/training example for short-horizon bag-stock depletion; generated data is illustrative only and has no validated predictive performance. Threshold-based replenishment remains the operational fallback. `software/mobile-app` is a React Native/Expo interface stub; it exposes site status and staff-confirmed service, not visitor features.

## Build, safety, and validation

BOM estimates are indicative, not quotations; enclosure, installation, batteries, and gateway excluded. Use low-voltage battery nodes only. Protect electronics from rain and vandalism with suitable enclosures and verify drainage, condensation, UV, and tamper resistance in a field trial. Do not install where a sensor obstructs public access. This system is advisory and cannot determine hygiene or safe access.

Reference schematics are textual pin/net plans in `schematic/`; no fabricated KiCad files are claimed. Firmware targets ESP-IDF for ESP32-C3 and nRF5 SDK for the beacon; board builds and RF interoperability have not been run. Verify BOM/pin compatibility, radio regulations for deployment region, ingress protection, and accessibility with the site operator before installation. No production security, OTA, or runtime claims are made.

## Status / roadmap

Implemented artifacts are an interface reference and runnable backend/ML examples. Hardware is unbuilt; firmware is skeletal; the app is a stub. Next: verify actual board pinouts, create reviewed KiCad schematics, build and exercise node firmware, test authenticated MQTT/TLS and offline replay, and run a supervised site pilot measuring false advisories and maintenance time saved.

## Cost target

Indicative electronics: gateway US$15–25; each sensor node US$8–18; inspection beacon US$5–12, excluding enclosure and labor. Costs vary by supplier and quantity; no savings claim is established.
