# SchoolSync API Spec

## REST endpoints

### `GET /api/v1/health`
Returns service health and current UTC timestamp.

### `GET /api/v1/overview`
Returns current readiness state, active alerts, per-child status cards, and lunch safety summaries.

### `POST /api/v1/events/routine`
Ingests a normalized routine event from the hub or test harness.

Request body:
```json
{
  "child_id": "ava",
  "event_type": "door_exit",
  "timestamp": "2026-08-22T06:52:00Z",
  "details": {"bag_present": true, "lunch_present": false}
}
```

### `POST /api/v1/predict/readiness`
Returns readiness score and lateness probabilities.

### `POST /api/v1/predict/lunch-safety`
Returns projected safe-until timestamp and risk category.

### `POST /api/v1/predict/route-anomaly`
Returns anomaly score and recommended escalation.

### `GET /api/v1/children/{child_id}/timeline`
Returns today's routine events for a child.

## WebSocket

### `/ws/live`
Broadcasts:
- `overview_update`
- `alert`
- `route_event`
- `device_health`
