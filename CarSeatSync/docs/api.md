# CarSeatSync API

## Endpoints

### `GET /health`
Returns service liveness.

### `POST /telemetry`
Accepts one normalized telemetry event.

### `GET /risk/summary?child_id=<id>`
Returns current explainable risk scores:
- `heat_risk`
- `harness_risk`
- `handoff_risk`
- `risk_level`

### `GET /children/{child_id}/timeline`
Returns all recorded events for demo/testing.

### `GET /recommendations?child_id=<id>`
Returns action list for caregivers.
