# RehabSync — API Specification

## Base URL

```
https://api.rehab-sync.io/api/v1
```

## Authentication

All endpoints require Bearer token:
```
Authorization: Bearer <JWT_TOKEN>
```

## Endpoints

### Health
| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Service health check |

### Exercises
| Method | Path | Description |
|--------|------|-------------|
| GET | `/exercises` | List all 30 exercises |
| GET | `/exercises/{id}` | Get exercise details |

### Patients
| Method | Path | Description |
|--------|------|-------------|
| POST | `/patients` | Create patient |
| GET | `/patients/{id}` | Get patient profile |
| GET | `/therapists/{id}/patients` | List therapist's patients |

### Exercise Plans
| Method | Path | Description |
|--------|------|-------------|
| POST | `/exercise-plans` | Create exercise plan |
| GET | `/exercise-plans/{patient_id}` | Get patient's current plan |

### Sessions
| Method | Path | Description |
|--------|------|-------------|
| POST | `/sessions` | Start exercise session |
| GET | `/sessions/{id}` | Get session details |
| GET | `/sessions?patient_id={id}` | List patient sessions |
| POST | `/sessions/{id}/end` | End session |

### Telemetry
| Method | Path | Description |
|--------|------|-------------|
| POST | `/telemetry` | Receive telemetry from Hub |

### Recovery & Analytics
| Method | Path | Description |
|--------|------|-------------|
| GET | `/recovery-forecast/{patient_id}` | 8-week recovery forecast |
| GET | `/adherence/{patient_id}` | Adherence metrics (7-day) |
| GET | `/form-trends/{patient_id}` | Form score trends |
| GET | `/rom-progress/{patient_id}` | Range-of-motion progress |

### Alerts
| Method | Path | Description |
|--------|------|-------------|
| POST | `/alerts` | Create alert |
| GET | `/alerts?patient_id={id}` | List alerts |

### Reports
| Method | Path | Description |
|--------|------|-------------|
| GET | `/reports/{patient_id}` | Generate clinical PDF report |

### OTA Firmware
| Method | Path | Description |
|--------|------|-------------|
| GET | `/ota/check/{node_type}` | Check for firmware update |
| POST | `/ota/firmware` | Upload firmware image |

### WebSocket
| Path | Description |
|------|-------------|
| `WS /ws/realtime/{patient_id}` | Real-time session data stream |

## Data Models

### TelemetryEntry
```json
{
  "hub_id": "hub_001",
  "patient_id": "patient_001",
  "timestamp": 1722259200,
  "session_id": "session_000123",
  "exercise": 12,
  "reps": 8,
  "form_score": 85,
  "form_deviation": 0,
  "joint_angles": {"knee_flexion": 92.5, "shoulder_flexion": 120.0},
  "force_mg": 2500000,
  "weight_g": 75000,
  "asymmetry": 50,
  "sensors_connected": 4,
  "band_connected": true,
  "mat_connected": true
}
```

### RecoveryForecast
```json
{
  "patient_id": "patient_001",
  "current_week": 6,
  "milestones": [
    {
      "name": "Independent ambulation",
      "target_days": 7,
      "predicted_days": 6,
      "status": "achieved",
      "confidence": 0.95
    }
  ],
  "overall_progress": 0.65,
  "adherence_rate": 0.78,
  "avg_form_score": 82,
  "risk_flags": []
}
```