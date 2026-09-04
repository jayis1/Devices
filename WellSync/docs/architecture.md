# WellSync Architecture

## End-to-end data flow

1. Mechanical-room nodes publish chemistry, pressure, and pump telemetry over RS-485 or 868 MHz.
2. The Hub normalizes all readings into signed WellSync frames and appends them to the edge historian.
3. Risk engines compute contamination, pump failure, dry-well, and treatment-integrity scores.
4. The Hub raises local state immediately even if WAN is down.
5. Cloud services ingest MQTT events, store time series, and expose user/service APIs.
6. The mobile app subscribes to live state changes and advisory actions.

## Edge responsibilities

- deterministic water-state transitions;
- local buffering during internet outage;
- OTA manifest fan-out to MCU nodes;
- installer provisioning and calibration cache;
- whole-home advisory relay output;
- local evidence snapshots for service calls.

## Cloud responsibilities

- long-term model retraining;
- fleet health and consumable analytics;
- homeowner notifications and digests;
- service-company multi-house dashboards;
- export of water-quality and maintenance reports.

## State machine

- `safe` — chemistry and treatment normal.
- `watch` — trend drift, freeze risk, or minor pump anomalies.
- `treat` — corrective action required before regular use.
- `do_not_drink` — severe chemistry/turbidity/treatment failure or dry-run event.

## Topic map

- `wellsync/hub/state`
- `wellsync/water-quality/raw`
- `wellsync/pump/state`
- `wellsync/tap/<tap_id>/event`
- `wellsync/weather/state`
- `wellsync/alert/advisory`
- `wellsync/command/<node_id>`
- `wellsync/ota/status`

## Database entities

- households
- nodes
- telemetry_samples
- service_logs
- advisories
- model_versions
- consumables
- lab_tests
