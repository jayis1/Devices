# UroSync API Spec

## GET /api/v1/health
Returns service status and current UTC timestamp.

## GET /api/v1/overview
Returns current fused risk summary.

### Example response
```json
{
  "hydration_risk": 0.62,
  "uti_risk": 0.31,
  "fall_risk": 0.44,
  "nocturia_forecast_next_7d": [2.1, 2.2, 2.4, 2.5, 2.7, 2.8, 3.0],
  "recommendations": ["Drink 300 mL water now and distribute the next 900 mL over 6 hours."]
}
```

## POST /api/v1/trend
Accepts 7-day time-series vectors and returns dehydraton / UTI / stone-adherence estimates.

## GET /api/v1/alerts
Returns active rule-based or inferred alerts.

## WebSocket /ws/live
Broadcast event channel with message types:
- `void`
- `mat`
- `bottle`
- `env`
