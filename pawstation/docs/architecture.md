# PawStation system architecture

## User and operational need

Park maintenance staff currently rely on visitor complaints and periodic walks to learn that bag dispensers are empty or bins need servicing. PawStation reports coarse station-level evidence to prioritize those checks. Pilot success metric: proportion of reported empty/full conditions confirmed by staff and time from observation to service; targets must be set with the operator, with no performance claim before field data.

## Data flow and offline behavior

Battery nodes sample locally and advertise bounded events; the gateway deduplicates and queues events locally when upstream connectivity is absent. Reconnection forwards queued events with original sequence identity. Staff confirmation is an explicit physical action. The demonstration backend uses SQLite. No raw audio, images, visitor identity, precise person location, or animal identity is collected. Sensor records are retained locally until operator-configured deletion; retention enforcement is not yet implemented.

## Threat and failure notes

Lost radio means stale status, which is displayed as unknown rather than inferred available. Dead batteries, blocked optical windows, lid movement, gateway/power failure, clock loss and internet outage can suppress or delay events; staff inspection remains authoritative. Public deployment requires authenticated device onboarding, TLS, per-site authorization, replay-safe message authentication, secure signed updates, and tamper-resistant enclosures; these are not implemented in the reference code.

## Power / regulatory

AA cells drive sensor nodes through a regulator chosen after measuring radio and sensor peaks. Runtime cannot be estimated credibly until the final duty cycle, battery chemistry, regulator quiescent current and enclosure temperatures are characterized. CR2032 beacon is only a candidate pending advertising-current tests. Confirm radio module approvals and applicable regional RF rules; no certification is claimed.

## Validation matrix

| Layer | Current evidence | Remaining |
|---|---|---|
| Backend | Python syntax and endpoint contract | dependency install, API tests, auth/TLS, MQTT integration |
| ML | deterministic synthetic generator/baseline | real labeled data, temporal holdout, metrics, drift |
| Firmware | protocol header and illustrative C skeleton | SDK builds, sensor/radio drivers, target tests |
| Hardware | textual reference net plan and candidate BOM | actual schematics, ERC, PCB, bench power/RF tests |
| App | accessible-label stub | Expo build, API wiring and device testing |
