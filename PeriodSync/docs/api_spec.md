# PeriodSync API Specification

## REST endpoints

### `GET /health`
Returns service status, model registry summary, and MQTT connectivity state.

### `GET /users/{user_id}/dashboard`
Returns the aggregated mobile dashboard payload.

### `GET /users/{user_id}/risk`
Returns current risk scores:
- `phase_confidence`
- `period_onset_7d`
- `leak_risk_60m`
- `cramp_score_120m`
- `iron_watch`

### `POST /telemetry`
Ingests normalized node telemetry.

Example body:
```json
{
  "user_id": "demo-user",
  "node": "temp-patch",
  "ts": "2026-08-23T07:30:00Z",
  "metrics": {
    "skin_temp_c": 36.7,
    "resting_hr": 63,
    "hrv_rmssd": 41.2
  }
}
```

### `POST /symptoms`
Stores symptom journaling data and optional interventions.

### `POST /commands/relief-belt`
Queues a local command for heat / haptic actuation through the hub.

### `GET /reports/{user_id}`
Returns a report payload suitable for export to PDF.

## WebSocket

### `/ws/{user_id}`
Pushes live updates:
- dashboard snapshots
- node battery changes
- leak alerts
- relief session state
- risk score updates

## MQTT mapping

Telemetry JSON is published to:
- `periodsync/{home_id}/telemetry/{node_id}`

Commands are published to:
- `periodsync/{home_id}/commands/{node_id}`

Acknowledgements are published to:
- `periodsync/{home_id}/acks/{node_id}`
