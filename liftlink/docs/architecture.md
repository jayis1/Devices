# LiftLink architecture

## Requirements

Advisory status only; no connection to elevator controller or safety circuit. 2+ lift sentinels feed a site hub, with a landing beacon for phone-free user/staff report. Target indoor BLE star; Wi-Fi/MQTT cloud is optional. Hub caches last state and queues a bounded event history while offline.

## Data flow

Door switch transitions and vibration summary -> sentinel event -> hub deduplicates and applies freshness rules -> local display and SQLite queue -> authenticated MQTT/TLS -> FastAPI event validation -> mobile status view. User/staff button events are marked unverified until an authorized staff acknowledgement.

## Power and operational assumptions

Sentinel budget estimate 0.25 mA average with a 3000 mAh cell: 3000 / 0.25 / 24 = 500 ideal days; 50% derating gives 250 days. Beacon 0.12 mA on 2000 mAh gives 694 ideal days, 347 days derated. Values are hypotheses; measure sleep, radio bursts, temperature and battery aging. Hub continuous 5 V/2 A. Low battery and missed heartbeat cause `unknown` status.

## Fault policy

Hub loss: sensors continue bounded local advertising; beacon gives local acknowledgement tone, no remote status. Internet loss: local status and queue continue. Sensor loss, stale timestamp (>5 minutes), disagreement, or reboot without fresh state: show unknown. Queue cap 1000 events; drop oldest telemetry (never a staff confirmation) and record loss counter. Nothing actuates the lift.

## Threat/privacy outline

Threats include rogue BLE publisher, stolen hub, hostile LAN client and malicious MQTT publisher. Prototype pairing is incomplete; production requires per-device enrollment, BLE authenticated pairing, TLS, broker topic ACLs, API auth, bounded payloads, rate limits, credential rotation and revocation. No PII/audio/video. Raw sensor data remains local and is discarded after aggregation; event retention target 30 days.

## Maturity

Concept/reference implementation only. Pinouts are board-level proposals. No KiCad capture/ERC, safety review, RF compliance, environmental testing, or station pilot.
