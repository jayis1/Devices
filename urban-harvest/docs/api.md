# UrbanHarvest — API Reference

## Base URL

```
http://urbanharvest.local:8000/api
```

## Endpoints

### Garden

| Method | Path | Description |
|--------|------|-------------|
| GET | `/garden/summary` | Garden-wide health dashboard |
| GET | `/plants` | List all plants |
| POST | `/plants` | Register new plant |
| GET | `/plants/{id}` | Plant detail + latest readings |
| GET | `/plants/{id}/readings?hours=24` | Time-series sensor data |
| POST | `/plants/{id}/water` | Manual irrigation command |
| POST | `/plants/{id}/nutrient` | Manual nutrient dosing |

### Grow Pod

| Method | Path | Description |
|--------|------|-------------|
| POST | `/growpod/light` | Set LED spectrum (R/B/W/FR) |

### Weather

| Method | Path | Description |
|--------|------|-------------|
| GET | `/weather` | Current outdoor conditions |
| GET | `/weather/forecast` | External forecast + correction |

### Alerts

| Method | Path | Description |
|--------|------|-------------|
| GET | `/alerts?acknowledged=false` | List active alerts |
| POST | `/alerts/{id}/acknowledge` | Acknowledge alert |
| POST | `/alerts/config` | Configure alert thresholds |

### Harvest

| Method | Path | Description |
|--------|------|-------------|
| GET | `/harvest/predictions` | Upcoming harvest dates |
| GET | `/planting/advice?lat=40.7&lon=-74.0` | What to plant now |

### Real-time

| Method | Path | Description |
|--------|------|-------------|
| WebSocket | `/ws/live` | Real-time sensor data stream |

## Data Models

### Plant Create

```json
{
  "name": "Cherry Tomato",
  "plant_type": "tomato",
  "location": "balcony",
  "sensor_node_id": 1,
  "pot_size_liters": 15.0
}
```

### Irrigation Command

```json
{
  "plant_id": 1,
  "volume_ml": 250,
  "immediate": true
}
```

### Light Command

```json
{
  "pod_id": 1,
  "red": 80,
  "blue": 70,
  "white": 60,
  "far_red": 20
}
```

### Alert Config

```json
{
  "plant_id": 1,
  "moisture_low": 20.0,
  "moisture_high": 80.0,
  "ec_max": 3.0,
  "temp_min": 10.0,
  "temp_max": 35.0
}
```

## Error Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 201 | Created |
| 400 | Bad request (invalid parameters) |
| 404 | Plant/alert not found |
| 500 | Internal server error |

## MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `urbanharvest/sensors/{plant_id}` | Hub → Cloud | JSON sensor reading |
| `urbanharvest/weather` | Hub → Cloud | JSON weather data |
| `urbanharvest/alerts` | Hub → Cloud | JSON alert |
| `urbanharvest/growpod/cmd` | Cloud → Hub | JSON command |
| `urbanharvest/growpod/status` | Hub → Cloud | JSON status |