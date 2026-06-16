# PowerPulse API Specification

Base URL: `http://powerpulse.local:8000/api/v1`

## Authentication
Currently no authentication (local network). Production deployments should add API key or OAuth.

## Endpoints

### Energy

#### GET /energy/circuits
Get per-circuit power data.

**Query Parameters:**
- `circuit_id` (optional): Filter by circuit ID (0-15)
- `start` (optional): Start timestamp (ISO 8601)
- `end` (optional): End timestamp (ISO 8601)
- `limit` (optional): Max results (default: 100, max: 10000)

**Response:**
```json
[
  {
    "timestamp": "2024-01-15T14:30:00Z",
    "circuit_id": 1,
    "voltage_mv": 121500,
    "current_ma": 5420,
    "power_w": 625,
    "power_factor": 0.95,
    "energy_wh": 15230
  }
]
```

#### GET /energy/appliances
Get per-appliance tag data.

**Query Parameters:**
- `tag_id` (optional): Filter by tag ID
- `start`, `end`, `limit`: Same as circuits

#### GET /energy/total
Get real-time aggregate power flow data.

**Response:**
```json
{
  "total_consumption_w": 3450,
  "solar_production_w": 2800,
  "grid_import_w": 650,
  "grid_export_w": 0,
  "battery_charge_w": 0,
  "battery_soc_pct": 72,
  "circuit_breakdown": {
    "0": 625, "1": 1200, "2": 450, "3": 875, "5": 300
  },
  "appliance_breakdown": {
    "1": 450, "3": 875, "5": 300
  }
}
```

### Alerts

#### GET /alerts
List alerts.

**Query Parameters:**
- `alert_type` (optional): Filter by type (arc_fault, overload, anomaly)
- `severity` (optional): Minimum severity (1-4)
- `acknowledged` (optional): Filter by acknowledgment status
- `limit` (optional): Max results (default: 50)

#### POST /alerts/{alert_id}/acknowledge
Acknowledge an alert.

**Response:**
```json
{ "status": "acknowledged", "alert_id": 42 }
```

### Devices

#### GET /devices
List all registered nodes.

#### POST /devices/{device_id}/command
Send a command to a node.

**Request Body:**
```json
{
  "type": "appliance_cmd",
  "tag_id": 3,
  "relay_cmd": 1,
  "schedule_type": 0,
  "schedule_time": 0,
  "duration_min": 0
}
```

### Solar

#### GET /solar/production
Get solar production data.

#### GET /solar/battery
Get current battery status.

**Response:**
```json
{
  "voltage_mv": 53200,
  "soc_pct": 72,
  "charge_mode": 1,
  "mppt_duty_pct": 68,
  "heatsink_temp_c": 42
}
```

### Automation

#### POST /automation/rules
Create an automation rule.

**Request Body:**
```json
{
  "name": "Peak hour shedding",
  "trigger_type": "time",
  "trigger_config": {
    "hours": [12, 13, 14, 15, 16, 17, 18],
    "days": ["mon", "tue", "wed", "thu", "fri"]
  },
  "action_type": "relay_toggle",
  "action_config": {
    "tag_id": 5,
    "relay_state": false,
    "reason": "Peak hours - shedding non-critical load"
  }
}
```

### Billing

#### GET /billing/estimate
Get estimated monthly bill.

**Response:**
```json
{
  "period_start": "2024-01-01T00:00:00Z",
  "period_end": "2024-01-15T14:30:00Z",
  "total_kwh": 342.5,
  "estimated_cost": 41.10,
  "breakdown": {
    "total_kwh": 342.5,
    "rate_per_kwh": 0.12,
    "days_remaining": 16
  }
}
```

#### GET /billing/tou-schedule
Get time-of-use rate schedule.

#### POST /billing/optimize
Get load-shifting recommendations.

### WebSocket

#### WS /ws/realtime
Real-time power data streaming.

**Messages (server → client):**
```json
{
  "type": "power_update",
  "timestamp": "2024-01-15T14:30:00Z",
  "total_watts": 3450,
  "solar_watts": 2800,
  "circuits": { "0": 625, "1": 1200 }
}
```

```json
{
  "type": "alert",
  "alert_type": "arc_fault",
  "severity": 4,
  "circuit_id": 3,
  "message": "Arc fault detected on circuit 3"
}
```

## Error Responses

All endpoints return standard HTTP error codes:
- `400`: Bad request (invalid parameters)
- `404`: Resource not found
- `422`: Validation error
- `500`: Internal server error

```json
{
  "detail": "Circuit ID must be between 0 and 15"
}
```