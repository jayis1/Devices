# QuietPath — community noise context and quiet-route guidance

## Problem and users
People who are noise-sensitive, work shifts, study, or care for young children often cannot tell whether a nearby walking route or shared outdoor space is temporarily loud before setting out. QuietPath gives a resident or site operator coarse, time-bucketed sound-level context from several fixed points and suggests a comparatively quieter mapped route. It does not identify people, recognize speech, measure legal compliance, or promise a quiet route. A lone phone cannot provide simultaneous measurements across a route; distributed sentinels plus a coordinating gateway are essential.

This differs from SoundNest (indoor personal acoustics), EchoSync (indoor sound-event accessibility alerts), and GlowRoute (outdoor lighting readiness): QuietPath reports outdoor aggregate sound exposure, has no audio classification, and does not control infrastructure. First-week value: check a neighborhood map before a walk and choose the less noisy of two paths. Local gateway continues sampling without internet; cloud is optional history and aggregation.

## System architecture

```text
QP-1 boundary sound sentinel -- LoRa 915/868* --> QP-0 site hub --> local API/map
QP-2 boundary sentinel      -- LoRa             -->     |         MQTT/TLS (optional)
QP-3 crossing sentinel      -- LoRa             -->     +--------> cloud + RN app
```

Nodes are QP-0 hub (Raspberry Pi Zero 2 W + SX1262 concentrator interface), QP-1/2/3 acoustic sentinels (ESP32-C3 + I2S digital microphone). Star LoRaWAN/private LoRa deployment is an integration choice; this reference uses addressed LoRa point-to-point frames, not LoRaWAN. Regional frequency, duty cycle, antenna and output power must be configured legally; 868 MHz EU / 915 MHz US variants are not interchangeable. BLE commissioning is optional and not implemented in the reference firmware. Maximum reference payload is 32 bytes; transmit one 10-minute aggregate, never audio.

## Privacy, safety, and limits
I2S samples are reduced on the sentinel to A-weighted-equivalent-level estimate and peak; audio buffers are volatile and discarded immediately. No recording, speech, identity, or occupant tracking. Published buckets are rounded to 5 dB, delayed by 10 minutes, and require at least two valid sentinels; avoid placing nodes at windows or near private patios. Hub retention default 30 days, configurable to zero. Public display is an indicative comparison, not a calibrated sound-level meter, health advice, nuisance finding, or regulatory evidence. Community consent and site-owner approval required. All nodes use SELV low-voltage power; no public-safety control or actuation.

## Node design and power

| Node | Core / components | Interfaces and assigned pins | Power target |
|---|---|---|---|
| QP-0 hub | Pi Zero 2 W, SX1262 SPI radio module, microSD | SPI radio via USB/SPI HAT; Wi-Fi Ethernet bridge; 5V USB-C | 5 V, 0.4–1.2 A estimated |
| QP-1..3 sentinel | ESP32-C3-MINI-1, INMP441 I2S mic, SX1262 module, MCP1700-3302 regulator | I2S BCLK GPIO4, WS GPIO5, SD GPIO6; radio SPI SCK GPIO2, MOSI GPIO3, MISO GPIO7, CS GPIO10, DIO1 GPIO1, RESET GPIO0; status LED GPIO8 | 5V USB or protected 1-cell LiFePO4 + regulated 3.3V; 25mA active estimate, <150uA sleep target, not measured |

Pin allocation is a proposed module-level assignment; verify module pin availability, strapping behavior, radio module logic level, and antenna clearance against chosen board datasheets before layout. Avoid sharing SPI and I2S pins. Microphone requires 3.3V; place 100nF + 10uF decoupling and acoustic port windscreen. No battery runtime is claimed: radio transmit peaks and regulator quiescent current require measurement.

## Protocol

Binary frame: `version:u8=1 | kind:u8 | node:u16le | seq:u16le | epoch_min:u32le | leq_cdb:u16le | peak_cdb:u16le | flags:u8 | crc16:u16le`. `leq_cdb` and `peak_cdb` are centi-dB estimates; `0xffff` invalid. CRC-16/CCITT-FALSE covers all preceding bytes. Sequence is monotonic per boot; receiver deduplicates `(node,seq)` and rejects timestamps older than 20 minutes or implausibly future. CRC is corruption detection only, not authentication. Production deployments MUST use LoRaWAN OTAA or an authenticated/encrypted link with per-node keys; never act on this telemetry. Lost packets are not retransmitted; next aggregate recovers. Clock uncertainty and sensor failure flags are retained. Python parser mirrors frame validation.

## Software

- `firmware/common/quietpath.c`: portable frame encode/decode and CRC, host-testable. `firmware/sentinel/` is an ESP-IDF-oriented acquisition stub; target build and microphone calibration are not supplied.
- `software/dashboard/`: FastAPI telemetry validation plus optional MQTT subscription adapter; local-only defaults, in-memory demo store. MQTT configuration requires TLS and credentials supplied via environment; no production auth/DB included.
- `software/ml-pipeline/`: synthetic demonstration dataset generator and baseline route-bucket comparison. Synthetic data is not evidence of accuracy; no pretrained model or claim of prediction.
- `software/mobile-app/`: React Native scaffold presenting route sound buckets and stale-node warnings; not a release-ready app.

## BOM (rough USD, single-unit estimates)

| Node | Estimated subtotal |
|---|---:|
| Hub QP-0 | $35–60 |
| Sentinel QP-1..3 each | $22–38 |
| Three-sentinel pilot total | $101–174 |

See `hardware/bom/` for preliminary parts. Estimates exclude enclosure, installation, calibration equipment, network and labor. Part availability and exact radio module variant must be checked at procurement.

## Bring-up
1. Select legal regional radio band and antenna; obtain site consent and choose non-residential-facing placements.
2. Assemble a sentinel, check rails and programming access, then flash a board-specific ESP-IDF application (hardware driver integration is not included).
3. Calibrate microphone response against a reference acoustic calibrator / sound-level meter in the installed windscreen and orientation; record offset and uncertainty.
4. Run host protocol tests and API tests; then collect multiple days of co-located comparison readings before showing public data.
5. Configure gateway local-only first; enable MQTT only with TLS verification, unique credentials, ACLs, and retention policy.

## Readiness and limitations

| Artifact | State |
|---|---|
| Architecture, pin assignments, BOM | Reference proposal; electrically unverified |
| C protocol codec | Implemented and host-testable |
| Sentinel firmware | Acquisition/application scaffold; no target SDK build |
| Hub radio integration | Interface design only; not implemented |
| Backend | Local ingestion demo; optional MQTT adapter |
| ML | Synthetic baseline only; not validated |
| Mobile | UI/API scaffold; not built |
| Schematics | Pin/net design notes, not KiCad or fabrication-ready |
| Physical system | Not assembled, calibrated, or field-tested |

Never describe this as a calibrated meter or regulatory monitoring system. On microphone fault, sentinel marks sample invalid; gateway displays stale/unknown rather than quiet. On hub/network loss, nodes keep sampling locally only if storage is configured; this reference does not implement persistent buffering. Reboot resets sequence; receiver starts a new session window. No remote commands or actuators exist.
