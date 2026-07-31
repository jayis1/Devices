# BloomSync — API Specification

## Base URL

```
https://api.bloom-sync.io/api/v1
```

## Authentication

JWT Bearer token. Roles: `patient`, `partner`, `obstetrician`, `admin`.

## Endpoints

### Health
| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Service health check |

### Patients
| Method | Path | Description |
|--------|------|-------------|
| POST | `/patients` | Create patient (postpartum profile) |
| GET | `/patients/{id}` | Get patient profile |
| PUT | `/patients/{id}` | Update patient profile |

### Vitals
| Method | Path | Description |
|--------|------|-------------|
| GET | `/vitals/{patient_id}` | Get vitals history (query: start, end, limit) |
| GET | `/vitals/{patient_id}/latest` | Get latest vitals snapshot |
| POST | `/telemetry` | Receive telemetry from Hub (MQTT bridge) |

### Nursing
| Method | Path | Description |
|--------|------|-------------|
| GET | `/nursing/{patient_id}` | Get nursing session history |
| GET | `/nursing/{patient_id}/today` | Get today's nursing log |
| GET | `/nursing/{patient_id}/stats` | Get nursing statistics (daily/weekly) |

### Wound
| Method | Path | Description |
|--------|------|-------------|
| GET | `/wound/{patient_id}` | Get wound monitoring data |
| GET | `/wound/{patient_id}/risk` | Get wound infection risk trend |

### Risk Assessment
| Method | Path | Description |
|--------|------|-------------|
| GET | `/risk/{patient_id}` | Get current risk assessment |
| GET | `/risk/{patient_id}/history` | Get risk history timeline |

### Recovery
| Method | Path | Description |
|--------|------|-------------|
| GET | `/recovery/{patient_id}/forecast` | Get 6-week recovery trajectory forecast |
| GET | `/recovery/{patient_id}/milestones` | Get recovery milestones |

### PPD Screening
| Method | Path | Description |
|--------|------|-------------|
| GET | `/ppd/{patient_id}/screen` | Get latest PPD screen result |
| GET | `/ppd/{patient_id}/history` | Get PPD screen history |
| POST | `/ppd/{patient_id}/voice` | Submit voice prosody for screening |

### Alerts
| Method | Path | Description |
|--------|------|-------------|
| GET | `/alerts/{patient_id}` | List alerts (query: severity, limit) |
| POST | `/alerts` | Create alert (from Hub or system) |
| PUT | `/alerts/{id}/ack` | Acknowledge alert |

### Obstetrician Dashboard
| Method | Path | Description |
|--------|------|-------------|
| GET | `/obstetricians/{id}/patients` | List obstetrician's patients |
| GET | `/obstetricians/{id}/overview` | Patient overview dashboard |

### Reports
| Method | Path | Description |
|--------|------|-------------|
| GET | `/reports/{patient_id}` | Generate clinical PDF report |

### OTA
| Method | Path | Description |
|--------|------|-------------|
| GET | `/ota/check/{node_type}` | Check for firmware updates |
| POST | `/ota/firmware` | Upload firmware image |

### WebSocket
| Path | Description |
|------|-------------|
| `/ws/realtime/{patient_id}` | Real-time vitals + alerts stream |

## Data Models

### Patient
```json
{
  "id": "patient_001",
  "name": "Sarah Johnson",
  "age": 32,
  "delivery_type": "cesarean",
  "delivery_date": "2026-07-01",
  "gestational_age_weeks": 39,
  "obstetrician_id": "obgyn_001",
  "parity": 1,
  "complications": ["gestational_diabetes"],
  "breastfeeding": true,
  "wound_type": "cesarean_incision",
  "recovery_day": 14
}
```

### Vitals
```json
{
  "patient_id": "patient_001",
  "timestamp": 1754000000,
  "heart_rate": 78,
  "spo2": 98,
  "skin_temp_c": 36.8,
  "hrv_rmssd_ms": 45,
  "activity_class": 0,
  "battery_pct": 85
}
```

### Risk Assessment
```json
{
  "patient_id": "patient_001",
  "timestamp": 1754000000,
  "hemorrhage_risk": 5,
  "preeclampsia_risk": 3,
  "wound_risk": 12,
  "mastitis_risk": 8,
  "ppd_risk": 15,
  "recovery_progress": 33,
  "overall_risk": 15,
  "alert_level": 0
}
```

### Recovery Forecast
```json
{
  "patient_id": "patient_001",
  "generated_at": "2026-07-31T10:00:00Z",
  "current_day": 30,
  "total_days": 42,
  "milestones": [
    {"name": "Pain-free ambulation", "target_day": 7, "predicted_day": 6, "status": "achieved", "confidence": 0.95},
    {"name": "Incision healing complete", "target_day": 21, "predicted_day": 20, "status": "achieved", "confidence": 0.90},
    {"name": "Sleep normalization", "target_day": 35, "predicted_day": 33, "status": "on_track", "confidence": 0.82},
    {"name": "Activity baseline restored", "target_day": 42, "predicted_day": 40, "status": "on_track", "confidence": 0.78}
  ],
  "overall_progress": 0.71,
  "risk_flags": []
}
```