# WellSync API Spec

## REST endpoints

### `GET /api/v1/health`
Returns service status and UTC time.

### `POST /api/v1/telemetry`
Accepts a unified telemetry event.

Payload:
```json
{
  "node_id": "inline-water-quality-1",
  "kind": "water_quality",
  "metrics": {"ph": 6.48, "turbidity_ntu": 2.2, "orp_mv": 202},
  "timestamp": "2026-09-04T00:00:00Z"
}
```

### `GET /api/v1/overview`
Returns household state, latest node snapshots, active advisories, and risk scores.

### `POST /api/v1/service-log`
Records maintenance actions such as filter replacement, shock chlorination, lab sample collection, or pump replacement.

### `GET /api/v1/risk/explain`
Returns human-readable reasoning for each risk score.

## WebSocket

### `GET /ws/live`
Pushes:
- `overview`
- `telemetry`
- `advisory`
- `service_log`
