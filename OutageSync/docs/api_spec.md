# OutageSync API Specification

## REST endpoints

- `GET /api/v1/health` — service health and DB path
- `GET /api/v1/overview` — outage forecast, load decisions, summary
- `GET /api/v1/panel` — recent panel telemetry
- `GET /api/v1/cold-chain` — recent fridge/freezer/medicine telemetry
- `GET /api/v1/outlets` — recent outlet telemetry
- `GET /api/v1/fuel` — recent fuel/generator telemetry

## WebSocket

- `WS /ws/live` — future live event channel for state changes and alerts

## Example overview response

```json
{
  "forecast": {
    "expected_minutes": 184,
    "confidence": 0.82,
    "strategy": "staged_generator_window"
  },
  "decisions": [
    {"label": "Router", "action": "keep_on", "rationale": "communications preserved"},
    {"label": "TV Console", "action": "shed", "rationale": "reserve under 60 minutes"}
  ],
  "summary": "Minimum cold hold time is 198 minutes; mean product temperature is -2.3°C."
}
```

## MQTT topic map

- `outagesync/node/<id>/panel`
- `outagesync/node/<id>/cold`
- `outagesync/node/<id>/outlet`
- `outagesync/node/<id>/fuel`
- `outagesync/hub/command/<id>`
