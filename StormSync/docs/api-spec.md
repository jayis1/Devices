# StormSync — API Specification

## Base URL

```
https://api.stormsync.cloud/api/v1
```

## Authentication

JWT Bearer token. Obtain via `/auth/login`.

## Endpoints

### Authentication

#### POST /auth/login
```json
// Request
{ "email": "user@example.com", "password": "..." }

// Response 200
{ "access_token": "eyJ...", "token_type": "bearer", "expires_in": 86400 }
```

### Devices

#### GET /devices
Returns list of all devices (nodes) in the system.

```json
[
  {
    "node_id": 0,
    "node_type": "hub",
    "name": "Hub",
    "battery_mv": 0,
    "last_seen": "2026-07-17T12:00:00Z",
    "online": true,
    "firmware_version": "1.0.0"
  }
]
```

#### POST /devices/{node_id}/ota
Trigger OTA firmware update.

### Sump Pit Data

#### GET /sump
Latest sump pit readings.

```json
{
  "node_id": 1,
  "timestamp": "2026-07-17T12:00:00Z",
  "water_level_mm": 350,
  "water_level_pct": 29,
  "pump_current_ma": 0,
  "pump_status": "off",
  "flow_rate_lpm": 0,
  "water_temp_c": 15.0,
  "vibration_rms_mg": 12,
  "vibration_peak_mg": 45,
  "mains_power": true,
  "pump_runtime_today_min": 23,
  "battery_v": 13.2
}
```

#### GET /sump/history?hours=24
Historical sump data (time series).

### Soil Data

#### GET /soil
Latest soil readings for all probes.

```json
[
  {
    "node_id": 2,
    "timestamp": "2026-07-17T12:00:00Z",
    "moisture_15_pct": 42.5,
    "moisture_45_pct": 68.3,
    "moisture_90_pct": 85.1,
    "pore_pressure_kpa": 12.5,
    "temp_15_c": 22,
    "temp_45_c": 18,
    "temp_90_c": 15,
    "battery_mv": 330
  }
]
```

### Weather

#### GET /weather
Current weather + forecast.

```json
{
  "timestamp": "2026-07-17T12:00:00Z",
  "temp_c": 22.3,
  "humidity_pct": 58.0,
  "pressure_hpa": 1008.2,
  "pressure_trend": "falling",
  "wind_speed_ms": 3.5,
  "wind_dir_deg": 180,
  "rain_mm": 2.4,
  "forecast": {
    "rain_6h_mm": 15.0,
    "rain_24h_mm": 32.0,
    "flood_watch": true
  }
}
```

### Flood Actuator

#### GET /actuator/status
```json
{
  "valve_status": "open",
  "pump_relay": false,
  "float_switch": false,
  "alarm_status": false,
  "mains_power": true,
  "battery_v": 13.1,
  "battery_health_pct": 95
}
```

#### POST /actuator/valve
```json
// Request
{ "action": "close" }  // or "open"

// Response 200
{ "status": "closed", "timestamp": "2026-07-17T12:00:00Z" }
```

#### POST /actuator/pump
```json
// Request
{ "action": "on" }  // or "off"

// Response 200
{ "status": "running", "timestamp": "2026-07-17T12:00:00Z" }
```

### Flood Score

#### GET /flood-score
```json
{
  "score": 35,
  "risk_level": "moderate",
  "confidence": 0.85,
  "factors": {
    "sump_water_level": "normal",
    "pump_health": "healthy",
    "soil_saturation": "elevated",
    "weather": "rain_expected",
    "pressure_trend": "falling"
  },
  "recommendations": [
    "Check gutters and downspouts",
    "Test backup pump",
    "Monitor forecast for heavy rain"
  ]
}
```

### Pump Health

#### GET /pump-health
```json
{
  "classification": "healthy",
  "confidence": 0.94,
  "predicted_time_to_failure_days": null,
  "vibration_trend": "stable",
  "current_draw_trend": "stable",
  "cycle_count_today": 12,
  "avg_cycle_duration_s": 35,
  "maintenance_recommended": false,
  "last_maintenance": "2026-01-15",
  "notes": "Pump operating within normal parameters"
}
```

### Flood Forecast

#### GET /flood-forecast
```json
{
  "forecast_horizon_hours": 6,
  "resolution_minutes": 15,
  "predictions": [
    {
      "timestamp": "2026-07-17T12:15:00Z",
      "water_level_mm": 350,
      "confidence_lower": 320,
      "confidence_upper": 380
    }
  ],
  "max_predicted_level_mm": 620,
  "max_predicted_time": "2026-07-17T15:30:00Z",
  "flood_threshold_mm": 1020,
  "flood_predicted": false
}
```

### Alerts

#### GET /alerts?acknowledged=false
```json
[
  {
    "id": "a1",
    "node_id": 1,
    "alert_type": "pump_degradation",
    "severity": 2,
    "message": "Sump pump bearing wear detected (class 1, 87% confidence)",
    "timestamp": "2026-07-17T10:00:00Z",
    "acknowledged": false
  }
]
```

#### PUT /alerts/{id}/ack

### WebSocket

#### WS /ws
Real-time updates (telemetry + alerts).

```json
{
  "type": "telemetry",
  "node_id": 1,
  "data": { "water_level_mm": 350, "pump_status": "off" }
}
```

```json
{
  "type": "alert",
  "data": { "alert_type": "high_water", "severity": 2, "message": "..." }
}
```