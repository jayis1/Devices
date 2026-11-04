# DryCycle — indoor laundry drying companion

Status: concept prototype specification by [jayis1](https://github.com/jayis1); not built, calibrated, or field-tested.

DryCycle helps apartment residents and households dry clothes indoors with less guesswork. A rack node measures hanging load change, a room sentinel observes temperature/humidity, and a gateway combines their time series into a transparent drying-progress estimate and ventilation advice. Unlike WashWise (washer/dryer operation) and MoldSync (building moisture/leak prevention), DryCycle observes passive air-drying and offers advisory guidance only. No camera, microphone, occupant tracking, HVAC control, or health claims.

## System contract

Target: households air-drying clothes in a single room. Demonstration workflow: place a known wet load on the instrumented rack, observe mass-loss and room humidity trends locally, then receive an approximate remaining-time band and a prompt to ventilate if conditions warrant. Gateway remains usable without internet; missing sensors yield “insufficient data,” never a fabricated drying estimate. One room per gateway; user starts and ends each load manually. Target sample interval 60 s; room sensor battery-life and wireless range are unverified. No automatic window or appliance control.

## Architecture

```text
Rack scale node (ESP32-C3 + load cell) -- BLE adv --+
Room climate node (ESP32-C3 + SHT31) ----- BLE adv --+--> Gateway (Pi Pico W)
                                                       |    local ring buffer
                                                       +--> FastAPI/MQTT --> mobile app stub
```

All radio nodes are low-voltage. BLE advertisements carry bounded, versioned measurements; gateway associates nodes by manually enrolled ID, deduplicates sequence numbers, and stores recent observations locally. Upstream MQTT is optional, TLS and broker authentication required in deployment. Cloud data is opt-in; default deployment is local-only.

## Hardware reference

| Node | Parts / interfaces | Power and safety |
|---|---|---|
| Gateway | Raspberry Pi Pico W; USB serial; Wi-Fi upstream, BLE via external USB adapter is an integration assumption | Regulated 5 V USB; no mains switching |
| Rack scale | ESP32-C3-MINI-1; HX711; 4× 50 kg load cells in bridge; BLE; I²C optional | 5 V HX711 analog supply, 3.3 V MCU; strain relief and mechanical overload stop required |
| Room sentinel | ESP32-C3-MINI-1; Sensirion SHT31-D; I²C SDA GPIO4/SCL GPIO5; BLE | 2×AA candidate; runtime unmeasured; sensor away from direct steam/sun |

Rack node reference pins: HX711 DOUT GPIO5, SCK GPIO6; load-cell bridge to HX711 E+/E-/A+/A-. Use a board/module with verified pin availability and 3.3 V logic compatibility. Four load cells form a full bridge; calibrate assembled mechanics with certified reference masses. Never support people or suspend loads overhead. Check HX711 module excitation/logic and cell wiring before connection. These are design references, not datasheet-verified production assignments.

## Protocol and data contract

Shared v1 JSON event: `{\"version\":1,\"node_id\":\"rack-1\",\"seq\":12,\"kind\":\"mass_g\",\"value\":1430,\"unit\":\"g\",\"utc_ms\":0}`. `mass_g` is integer 0–50000; `rh_pct` 0–10000 basis points; `temp_centi_c` −4000–8500. `utc_ms=0` means unknown; gateway records receipt time. Reject unknown version, oversized payload, invalid ranges, duplicate `(node_id,seq)`; buffer bounded latest 24 h. BLE transport framing/cryptographic pairing is not implemented; production deployments must use authenticated enrollment and encrypted upstream transport. Never treat advertisements as trusted actuator commands.

## Software

- `firmware/`: C interface examples; build targets and radio drivers are not yet implemented.
- `software/dashboard/`: FastAPI reference endpoint and optional MQTT requirements; local prototype only.
- `software/ml-pipeline/`: deterministic synthetic-data baseline example for drying-time bands. Synthetic data validates plumbing only, not accuracy. Required eventual training data: consented, timestamped load mass and room conditions across fabric, load, airflow, and household/site groups; split by household, report MAE and interval coverage. Rule-based mass-loss baseline remains fallback.
- `software/mobile-app/`: React Native/Expo UI stub, not a tested app.

## Power, failure behavior, and privacy

Prototype power budget is unmeasured. HX711 bridge excitation and radio peaks must be measured; avoid powering bridge continuously in battery mode until validated. On gateway restart, retain bounded local events if storage is configured; otherwise show stale/unknown status. Sensor loss >5 min invalidates estimate; clock skew is handled by gateway receipt time. Calibration expiry is user-visible and prompts re-weighing. No automatic control means communications failure cannot actuate equipment. Retain only load session summaries by default; provide deletion/export in a production implementation. Do not infer household occupancy.

## Contents and validation status

BOMs are indicative and exclude enclosure, wiring, load-cell mechanics, shipping, and labor. Schematic directory contains textual connection plans only; no native KiCad electrical design or ERC result is claimed. No firmware build, RF test, app build, load-cell accuracy test, or physical safety review has been performed. Component variants, connector ratings, BOM substitutions, and all pin assignments require datasheet and board-level review before assembly. This is not a mold-prevention product or a calibrated scale.

See `docs/architecture.md`, `docs/protocol.md`, and `docs/api.md`. All accuracy and energy-savings claims are intentionally withheld pending field measurements.
