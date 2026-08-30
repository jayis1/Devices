# DrainSync API

Base path: `/api/v1`

## Endpoints

### `GET /health`
Returns API status, database path, and UTC timestamp.

### `GET /overview`
Returns:
- latest backup forecast
- clog leaderboard by branch
- trap-prime recommendations
- valve state

### `GET /nodes`
Returns node inventory and last-seen state.

### `POST /telemetry/flow`
Ingests under-sink summary frames.

### `POST /telemetry/trap`
Ingests floor-drain status.

### `POST /telemetry/stack`
Ingests main-stack data.

### `POST /telemetry/actuator`
Ingests backwater actuator data.

### `GET /alerts`
Returns recent alert queue sorted by severity.

### `POST /commands/valve`
Issues open/close/hold command to actuator.

## MQTT topics

- `drainsync/<home_id>/<node_id>/flow`
- `drainsync/<home_id>/<node_id>/trap`
- `drainsync/<home_id>/<node_id>/stack`
- `drainsync/<home_id>/<node_id>/actuator`
- `drainsync/<home_id>/commands/valve`

## Example overview response

```json
{
  "backup_forecast": {"risk": 0.18, "state": "green", "hours_to_peak": 36},
  "valve": {"position": "open", "healthy": true},
  "top_clog_risks": [
    {"branch_id": "kitchen-west", "risk": 0.74},
    {"branch_id": "laundry", "risk": 0.42}
  ],
  "trap_recommendations": [
    {"node_id": "floor-2", "action": "prime", "volume_ml": 300}
  ]
}
```
