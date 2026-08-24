# FoodAllergySync API Spec

## REST endpoints

### `GET /health`
Returns service health and MQTT bridge status.

### `GET /profiles`
Returns all allergy profiles.

### `POST /profiles`
Create or update a profile.

Request body:
```json
{
  "name": "Maya",
  "allergens": ["peanut", "tree_nut", "sesame"],
  "severity": "high",
  "carry_required": true
}
```

### `POST /scan/package`
Submit a meal-scanner package event.

### `POST /scan/strip`
Submit strip-reader result payload.

### `POST /readiness/epipen`
Submit injector readiness event.

### `GET /dashboard/summary`
Returns household summary, current alerts, readiness scores, and recent events.

### `GET /alerts/active`
Returns unresolved alerts.

## MQTT topics

- `foodallergy/home/<id>/telemetry/<node>`
- `foodallergy/home/<id>/event/<node>`
- `foodallergy/home/<id>/command/<node>`
- `foodallergy/home/<id>/state/summary`
- `foodallergy/home/<id>/ota/<node>`

## Websocket

### `GET /ws`
Broadcasts summary snapshots and active alerts.
