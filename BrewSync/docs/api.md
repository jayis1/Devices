# BrewSync API Specification

## Base URL

```
Production: https://api.brewsync.io/v1
Local:      http://<hub-ip>:8080/v1
```

## Authentication

All endpoints require a JWT Bearer token obtained via `/auth/login`.

```
Authorization: Bearer <jwt_token>
```

## Endpoints

### Auth

#### POST /auth/login
```json
// Request
{
  "email": "brewer@example.com",
  "password": "securepassword"
}
// Response
{
  "access_token": "eyJ...",
  "token_type": "bearer",
  "expires_in": 86400
}
```

#### POST /auth/register
```json
// Request
{
  "email": "brewer@example.com",
  "password": "securepassword",
  "name": "Jane Brewer"
}
```

### Batches

#### GET /batches
List all batches for the authenticated user.

```json
// Response
{
  "batches": [
    {
      "id": "batch_abc123",
      "name": "West Coast IPA #5",
      "style": "American IPA",
      "status": "active_fermentation",
      "started_at": "2025-03-15T10:30:00Z",
      "vessel_id": "fermenter_001",
      "recipe": {
        "og": 1.065,
        "fg": 1.012,
        "abv_target": 6.9,
        "ibu_target": 65,
        "yeast": "US-05"
      }
    }
  ]
}
```

#### POST /batches
Create a new batch.

```json
// Request
{
  "name": "West Coast IPA #6",
  "style": "American IPA",
  "vessel_id": "fermenter_001",
  "recipe_xml": "<xml>...</xml>",  // BeerXML format
  "target_og": 1.065,
  "target_fg": 1.012,
  "temp_schedule": [
    {"days": 0, "temp_c": 18.3},
    {"days": 7, "temp_c": 20.0},
    {"days": 10, "temp_c": 4.0}
  ]
}
```

#### GET /batches/{batch_id}
Get batch details with current readings.

```json
// Response
{
  "id": "batch_abc123",
  "name": "West Coast IPA #5",
  "status": "active_fermentation",
  "current": {
    "sg": 1.0382,
    "temp_c": 19.1,
    "co2_ppm": 1245,
    "pressure_bar": 1.02,
    "ph": 4.12,
    "abv_current": 3.6,
    "attenuation_pct": 58.5
  },
  "predictions": {
    "estimated_fg": 1.013,
    "estimated_completion": "2025-03-25T14:00:00Z",
    "stuck_probability": 0.03,
    "infection_probability": 0.01
  },
  "timeline": [...]
}
```

### Readings

#### GET /batches/{batch_id}/readings
Get time-series readings for a batch.

**Query params**: `from`, `to`, `interval` (5m, 1h, 6h, 1d)

```json
// Response
{
  "readings": [
    {
      "timestamp": "2025-03-16T10:00:00Z",
      "sg": 1.0520,
      "temp_c": 18.5,
      "co2_ppm": 2340,
      "pressure_bar": 1.05,
      "ph": 4.45,
      "node_id": "fermenter_001"
    }
  ]
}
```

#### POST /batches/{batch_id}/readings
Ingest a reading from a node (used by Hub MQTT bridge).

```json
// Request
{
  "timestamp": "2025-03-16T10:05:00Z",
  "node_id": "fermenter_001",
  "sg": 1.0515,
  "temp_c": 18.6,
  "co2_ppm": 2280,
  "pressure_bar": 1.04,
  "ph": 4.44
}
```

### Scanner

#### POST /scanner/analyze
Submit spectral scan from Brew Scanner for analysis.

```json
// Request
{
  "batch_id": "batch_abc123",
  "scan_type": "refractometer",  // or "infection_check", "color", "full"
  "spectral_data": [/* 11-channel AS7341 readings */],
  "volume_ml": 19000,
  "notes": "Day 7 sample"
}
// Response
{
  "scan_id": "scan_xyz789",
  "estimated_og": 1.0648,
  "estimated_fg": null,
  "color_srm": 6.2,
  "estimated_ibu": 58,
  "infection_probability": 0.02,
  "infection_type": null,
  "volume_ml": 19000,
  "abv_current": 3.5,
  "recommendations": []
}
```

### Nodes

#### GET /nodes
List all registered nodes.

#### POST /nodes/register
Register a new node with the system.

```json
// Request
{
  "node_type": "fermenter",  // fermenter, cellar, scanner
  "node_id": "fermenter_001",
  "firmware_version": "1.2.0"
}
```

#### GET /nodes/{node_id}/status
Get node battery, signal strength, last seen, sensor health.

### Alerts

#### GET /alerts
List active and historical alerts.

```json
// Response
{
  "alerts": [
    {
      "id": "alert_001",
      "batch_id": "batch_abc123",
      "type": "temperature_excursion",
      "severity": "warning",
      "message": "Fermentation temperature 22.1°C exceeds schedule target of 20°C",
      "created_at": "2025-03-16T08:30:00Z",
      "acknowledged": false
    }
  ]
}
```

#### POST /alerts/{alert_id}/acknowledge

### ML Predictions

#### GET /batches/{batch_id}/predictions
Get latest ML predictions for a batch.

```json
// Response
{
  "fermentation_progress": {
    "estimated_fg": 1.013,
    "estimated_completion": "2025-03-25T14:00:00Z",
    "confidence": 0.89
  },
  "stuck_fermentation": {
    "probability": 0.03,
    "risk_factors": [],
    "recommendations": []
  },
  "infection_risk": {
    "probability": 0.01,
    "type": null,
    "recommendations": []
  },
  "yeast_health": {
    "estimated_cell_count_million_ml": 85,
    "viability_pct": 94,
    "health_score": 0.91
  },
  "flavor_profile": {
    "predicted_abv": 6.8,
    "predicted_ibu": 63,
    "predicted_srm": 6.0,
    "flavor_notes": ["citrus", "pine", "grapefruit"]
  }
}
```

### Recipes

#### POST /recipes/import
Import a BeerXML recipe file.

#### GET /recipes
List user's recipes.

#### GET /recipes/{recipe_id}/suggestions
Get AI-powered suggestions for temperature schedule, yeast pitch rate, and water chemistry.

## WebSocket

### WS /ws/batches/{batch_id}
Real-time fermentation data stream.

```json
// Messages
{"type": "reading", "data": {"sg": 1.0380, "temp_c": 19.1, "co2_ppm": 1200, "ts": "..."}}
{"type": "alert", "data": {"alert_type": "stuck_fermentation", "severity": "critical", "message": "..."}}
{"type": "prediction_update", "data": {"estimated_fg": 1.013, "stuck_probability": 0.05}}
{"type": "state_change", "data": {"from": "lag_phase", "to": "active_fermentation"}}
```

## MQTT Topics (Hub ↔ Cloud)

```
brewsync/{user_id}/nodes/{node_id}/telemetry     — Periodic sensor readings
brewsync/{user_id}/nodes/{node_id}/status          — Battery, signal, health
brewsync/{user_id}/batches/{batch_id}/readings     — Ingested readings
brewsync/{user_id}/batches/{batch_id}/commands      — Temp schedule updates, etc.
brewsync/{user_id}/alerts                           — Alert notifications
brewsync/{user_id}/ml/predictions                   — Prediction updates
```