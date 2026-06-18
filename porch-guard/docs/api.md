# PorchGuard — API Reference

Base URL: `http://<hub-ip>:8000`

## System Status

### GET /api/status
Overall system status. **Pirate risk is the headline number.**

```json
{
  "camera": { "presence_state": 3, "person_id": 4, "parcel_class": 2, "pirate_risk": 210, ... },
  "mailbox": { "mail_class": 1, "weight_mg": 12000, "battery_pct": 85, ... },
  "lock": { "lock_state": 0, "door_state": 0, "battery_pct": 92, ... },
  "pirate_risk": 0.82,
  "porch_active": true,
  "armed": true,
  "siren_active": false
}
```

## Camera

### GET /api/camera/latest
Latest camera telemetry.

### GET /api/camera/history?hours=24
Historical camera readings (default: last 24 hours).

## Mailbox

### GET /api/mailbox/latest
Latest mailbox telemetry.

## Lock

### GET /api/lock/latest
Latest lock telemetry (state, door, battery, codes active).

### GET /api/lock/history?limit=100
Historical lock readings.

## Pirate Risk

### GET /api/pirate_risk
Current pirate risk score + level + recent risk history.

```json
{
  "risk_score": 0.82,
  "level": "CRITICAL",
  "porch_active": true,
  "recent_risks": [12, 15, 18, 22, ...]
}
```

## Deliveries

### GET /api/deliveries?limit=50
Delivery log (parcel drops, mail arrivals/collections).

### GET /api/delivery_stats?days=30
Delivery summary with per-courier counts and theft rate.

## Alerts

### GET /api/alerts?limit=50
Recent alerts (pirate, tamper, delivery, door-left-open, etc.).

### POST /api/alerts/ack
Acknowledge an alert. Body: `{"alert_id": 12}`

## Lock Control

### POST /api/unlock
Unlock the door. Body: `{"source": 0, "code_id": 0}`

### POST /api/lock
Lock the door.

### POST /api/garage
Pulse garage door relay. Body: `{"duration_s": 1}`

## Courier Codes

### POST /api/codes/issue
Issue a one-time courier code. Body: `{"window_minutes": 60, "delivery_note": "Amazon delivery"}`

Returns: `{"status": "issued", "code_id": 5, "code": "482910", "valid_minutes": 60}`

### GET /api/codes/active
List active (unused, unrevoked) courier codes.

### POST /api/codes/revoke/{code_id}
Revoke a courier code.

## Arm/Disarm

### POST /api/arm
Arm or disarm the porch. Body: `{"armed": true}`

## Siren

### POST /api/siren
Trigger the 100 dB siren. Body: `{"duration_s": 15}`

### POST /api/siren/off
Stop the siren.

## Reference Data

### GET /api/couriers
Courier ID → name mapping.

### GET /api/persons
Person ID → name mapping.

## WebSocket

### WS /ws/live
Real-time push of `latest_data` every 1 second.