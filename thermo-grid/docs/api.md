# ThermoGrid — API Reference

Base URL: `http://<hub-ip>:8000`

## System Status

### GET /api/status
Overall system status with thermal map, energy, solar, and comfort.

```json
{
  "zones": [{"zone_id": 0, "setpoint": 21.0, "mode": 1, "boost_minutes": 0, ...}],
  "sensors": [{"node_id": 16, "zone_id": 0, "air_temp": 20.5, "mrt": 19.8, ...}],
  "actuators": [{"node_id": 32, "zone_id": 0, "valve_pos": 45, "pipe_temp": 35.0, ...}],
  "comfort": [{"person_id": 128, "skin_temp": 31.5, "comfort_score": 0, ...}],
  "solar": {"production_w": 1200, "surplus_w": 400, "boost_recommended": true},
  "tou": {"current_period": 0, "rate_cents": 8.0},
  "active_alerts": 2,
  "mqtt_connected": true
}
```

## Zones

### GET /api/zones
List all configured zones with current state.

### GET /api/zones/{zone_id}/history?hours=24
Historical sensor + actuator data for a zone.

### POST /api/zones/{zone_id}/setpoint
Set zone setpoint + mode. Body: `{"zone_id": 0, "setpoint": 21.5, "mode": 1, "boost_minutes": 0}`

### POST /api/zones/{zone_id}/boost
Temporary boost. Body: `{"delta": 1.5, "minutes": 30}`

### POST /api/zones/{zone_id}/schedule
Set hourly schedule. Body: `{"zone_id": 0, "slots": [{"hour": 6, "setpoint": 22.0}, ...]}`

## Comfort

### GET /api/comfort/{person_id}
Get personal comfort profile + vote history + statistics.

```json
{
  "person_id": 128,
  "latest": {"skin_temp": 31.5, "comfort_score": 0, "activity": 0, ...},
  "votes": [{"vote": -2, "skin_temp": 28.0, "timestamp": "..."}],
  "stats": {"total_votes": 45, "avg_vote": -0.3, "avg_skin_temp": 31.2}
}
```

### POST /api/comfort/{person_id}/vote
Submit a comfort vote. Body: `{"person_id": 128, "vote": -2, "skin_temp": 28.0, "activity": 0}`

## Energy

### GET /api/energy?days=7
Energy consumption per zone + total.

### GET /api/energy/savings?days=30
Compare actual usage to single-thermostat counterfactual.

```json
{
  "days": 30,
  "actual_wh": 45000,
  "counterfactual_wh": 64285,
  "savings_wh": 19285,
  "savings_pct": 30
}
```

## Solar

### GET /api/solar?hours=24
Solar production + self-consumption time series.

## Thermal Map

### GET /api/thermal_map
Current thermal map: per-room temp, MRT, occupancy, zone states.

## Forecast

### GET /api/forecast
4-hour thermal forecast per zone (from hub's ML model).

### POST /api/forecast/run
Trigger cloud-side thermal forecast model (heavier than on-hub version).

## Optimization

### POST /api/optimize
Run MILP energy optimization on cloud. Computes optimal zone setpoints for next 4 hours.

## Alerts

### GET /api/alerts?limit=50
Recent alerts (freeze, window, fault, sensor offline, etc.).

### POST /api/alerts/ack
Acknowledge an alert. Body: `{"alert_id": 12}`

## WebSocket

### WS /ws/live
Real-time push of `status` every 5 seconds + MQTT event messages.