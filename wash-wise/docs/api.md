# WashWise — API Reference

Base URL: `http://<hub-ip>:8000`

## System Status

### GET /api/status
Overall system status. **Fire risk is the headline number.**

```json
{
  "washer": { "cycle_phase": 2, "water_temp": 40.5, "reservoir_g": 450, ... },
  "dryer": { "exhaust_temp": 65.2, "fire_risk_score": 15, "lint_clog_level": 0, ... },
  "scanner": { "fabric_type": 1, "stain_type": 0, "detergent_ml": 35, ... },
  "fire_risk": 0.06,
  "dryer_active": false
}
```

## Washer

### GET /api/washer/latest
Latest washer telemetry.

### GET /api/washer/history?hours=24
Historical washer readings (default: last 24 hours).

## Dryer

### GET /api/dryer/latest
Latest dryer telemetry including `fire_risk_score` (0-255).

### GET /api/dryer/history?hours=24
Historical dryer readings.

## Fire Safety

### GET /api/fire_risk
Current fire risk score + level + recent exhaust temperatures.

```json
{
  "risk_score": 0.15,
  "level": "OK",
  "dryer_active": false,
  "recent_temps": [65.2, 64.8, 65.1, ...]
}
```

**Levels:** OK, WARNING (>0.6), CRITICAL (>0.8), EMERGENCY (>0.95)

## Scans

### GET /api/scans/latest
Most recent garment scan result.

### GET /api/scans/history?limit=50
Scan history.

```json
{
  "fabric_type": 1,
  "fabric_confidence": 230,
  "stain_type": 4,
  "stain_confidence": 200,
  "wash_temp": 40.0,
  "recommended_cycle": 0,
  "detergent_ml": 45
}
```

## Alerts

### GET /api/alerts?limit=50
Recent alerts (fire, leak, imbalance, dose, etc.).

### POST /api/alerts/ack
Acknowledge an alert.

```json
{ "alert_id": 42 }
```

## Commands

### POST /api/dose
Send detergent dosing command to washer node.

```json
{ "detergent_ml": 35, "reason": "manual" }
```

### POST /api/cycle_select
Select wash cycle.

```json
{ "cycle_type": 0, "temp_c": 40.0 }
```

## Energy

### GET /api/energy?days=30
Energy + water usage summary.

```json
{
  "cycles": 42,
  "total_energy_kwh": 15.6,
  "total_water_l": 840,
  "total_cost_dollars": 3.12,
  "total_co2_kg": 7.8
}
```

## Reference Databases

### GET /api/stains
All stain types with treatment recommendations.

### GET /api/stains/{stain_id}
Single stain detail.

```json
{
  "name": "Blood",
  "treatment": "COLD water only (heat sets blood permanently). Hydrogen peroxide on cotton. Wash cold.",
  "difficulty": "medium"
}
```

### GET /api/fabrics
All fabric types with care recommendations.

### GET /api/fabrics/{fabric_id}
Single fabric detail.

```json
{
  "name": "Cotton",
  "max_temp": 60,
  "cycle": "normal",
  "bleach": "ok",
  "tumble_dry": true
}
```

### GET /api/maintenance
Lint cleaning and maintenance log.

## WebSocket

### WS /ws/live
Real-time updates (all telemetry, pushed every 1 second).

## MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `washwise/washer_data` | node→cloud | Washer telemetry JSON |
| `washwise/dryer_data` | node→cloud | Dryer telemetry JSON |
| `washwise/scan_result` | node→cloud | Scan result JSON |
| `washwise/fire_alert` | node→cloud | Fire alert JSON (critical) |
| `washwise/energy_data` | node→cloud | Per-cycle energy JSON |
| `washwise/alerts` | hub→cloud | General alerts |
| `washwise/commands/dose` | cloud→hub | Dose command |
| `washwise/commands/cycle` | cloud→hub | Cycle selection |
| `washwise/commands/dryer_shutoff` | cloud→hub | Dryer shutoff advisory |