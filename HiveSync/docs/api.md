# HiveSync — API Specification

Base URL: `https://api.hivesync.io/v1`

Authentication: Bearer JWT token in `Authorization` header.

## Endpoints

### Apiaries

| Method | Path | Description |
|--------|------|-------------|
| GET | `/apiaries` | List all apiaries |
| POST | `/apiaries` | Create apiary |
| GET | `/apiaries/{id}` | Get apiary details |
| PATCH | `/apiaries/{id}` | Update apiary |
| DELETE | `/apiaries/{id}` | Delete apiary |
| GET | `/apiaries/{id}/dashboard` | Multi-hive dashboard |

### Hives

| Method | Path | Description |
|--------|------|-------------|
| GET | `/hives?apiary_id=` | List hives in apiary |
| POST | `/hives` | Register new hive |
| GET | `/hives/{id}` | Get hive details |
| GET | `/hives/{id}/health` | Composite health score (0-100) |
| GET | `/hives/{id}/swarm-risk` | 7-day swarm probability forecast |
| GET | `/hives/{id}/queen-status` | Queen health assessment |
| GET | `/hives/{id}/varroa` | Varroa mite trend |
| POST | `/hives/{id}/feed` | Command feeder |
| GET | `/hives/{id}/readings?metric=&hours=` | Time-series data |

### Readings

| Method | Path | Description |
|--------|------|-------------|
| POST | `/readings/batch` | Ingest batch sensor readings |
| POST | `/readings/entrance` | Ingest entrance monitor data |
| POST | `/readings/feeder` | Ingest feeder status |

### Alerts

| Method | Path | Description |
|--------|------|-------------|
| GET | `/alerts?severity=&acknowledged=` | List alerts |
| POST | `/alerts/{id}/acknowledge` | Acknowledge alert |
| POST | `/alerts/subscribe` | Subscribe push notifications |

### ML

| Method | Path | Description |
|--------|------|-------------|
| POST | `/ml/swarm-predict` | Run swarm prediction |
| POST | `/ml/varroa-detect` | Run Varroa detection on image |
| POST | `/ml/queen-health` | Assess queen health |

## Common Data Models

### SensorReading

```json
{
  "node_id": 1,
  "timestamp": "2025-06-26T14:30:00Z",
  "temp_brood_c": 35.2,
  "temp_top_c": 33.8,
  "temp_entrance_c": 28.1,
  "humidity_pct": 62.3,
  "weight_kg": 42.157,
  "weight_delta_g": -23.0,
  "accel_rms_mg": 12.4,
  "battery_mv": 3021,
  "spectral_centroid_hz": 245.0,
  "peak_freq_hz": 250.0,
  "peak_amplitude_db": -32.5,
  "spectral_bandwidth_hz": 85.0
}
```

### HiveHealth

```json
{
  "score": 78.5,
  "queen_status": "healthy",
  "swarm_risk": 0.12,
  "mite_level": "low",
  "weight_trend": "stable",
  "forager_traffic": "normal",
  "alerts": []
}
```

### SwarmRisk

```json
{
  "probability_24h": 0.05,
  "probability_72h": 0.18,
  "probability_7d": 0.32,
  "recommendation": "Monitor closely. Consider adding a super."
}
```

## Error Responses

All errors follow RFC 7807:

```json
{
  "type": "https://api.hivesync.io/errors/not-found",
  "title": "Hive Not Found",
  "status": 404,
  "detail": "Hive with ID 'abc123' does not exist."
}
```

## Rate Limits

- Free tier: 100 requests/minute
- Pro tier: 1,000 requests/minute
- Enterprise: Custom