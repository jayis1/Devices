# MoldSync Architecture

## Design goals

1. Detect moisture risk before visible mold appears.
2. Distinguish occupant moisture from plumbing or envelope failures.
3. Automate drying actions without creating noise fatigue.
4. Keep core safety and intervention logic working locally.

## Edge / cloud split

- **Hub edge layer** handles MQTT ingestion, policy rules, local scoring, and control actions.
- **Cloud layer** handles long-horizon forecasting, model retraining, fleet analytics, and secure remote access.
- **Mobile app** renders alerts, maps room health, and launches guided inspections.

## Data flow

1. Endpoints emit compressed telemetry every 60 s.
2. Hub stores recent data in SQLite and forwards to MQTT.
3. FastAPI service exposes latest state and prediction APIs.
4. Training jobs read parquet exports, fit models, and publish artifacts.
5. Hub periodically downloads signed models and uses them for inference fallback.

## Failure handling

- Loss of internet: local controls continue.
- Loss of hub heartbeat: vent controllers fall back to local RH thresholds.
- Radio congestion: urgent leak / alarm frames preempt routine telemetry.
- Sensor fault: system marks confidence degraded and avoids aggressive shutoff unless corroborated.
