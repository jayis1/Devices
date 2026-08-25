# MoldSync API Spec

## REST endpoints

### `GET /health`
Returns service health.

### `GET /rooms`
Returns current room states, condensation margin, and risk scores.

### `POST /telemetry/room`
Ingests room sentinel telemetry.

Payload:
```json
{
  "room_id": "bathroom-east",
  "air_temp_c": 24.1,
  "rh": 78.2,
  "surface_temp_c": 21.0,
  "co2_ppm": 912,
  "voc_index": 112,
  "moisture_pf": 0.64
}
```

### `POST /telemetry/plumbing`
Ingests plumbing state.

### `POST /inspect`
Returns damp-patch segmentation summary and remediation hints.

### `GET /alerts/active`
Lists active moisture, condensation, leak, and intervention alerts.

### `GET /summary`
Returns headline scores.
