# MenoSync — API Specification

## Base URL

```
https://api.menosync.io/api/v1
```

## Authentication

JWT Bearer token. Roles: `patient`, `gynecologist`, `admin`.

## Endpoints

### Health
| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Service health check |

### Patients
| Method | Path | Description |
|--------|------|-------------|
| POST | `/patients` | Create patient (menopause profile) |
| GET | `/patients/{id}` | Get patient profile |

### Vitals
| Method | Path | Description |
|--------|------|-------------|
| GET | `/vitals/{patient_id}` | Get vitals history |
| GET | `/vitals/{patient_id}/latest` | Get latest vitals snapshot |
| POST | `/telemetry` | Receive telemetry from Hub |

### EDA
| Method | Path | Description |
|--------|------|-------------|
| GET | `/eda/{patient_id}` | Get EDA/stress history |
| POST | `/eda` | Receive EDA data from Hub |

### Sleep
| Method | Path | Description |
|--------|------|-------------|
| GET | `/sleep/{patient_id}` | Get sleep data (BCG staging) |
| POST | `/sleep` | Receive sleep data from Hub |

### Night Sweat
| Method | Path | Description |
|--------|------|-------------|
| GET | `/sweat/{patient_id}` | Get night sweat readings |
| POST | `/sweat` | Receive sweat data from Hub |

### Ambient
| Method | Path | Description |
|--------|------|-------------|
| POST | `/ambient` | Receive ambient data from Hub |

### Hot Flash
| Method | Path | Description |
|--------|------|-------------|
| GET | `/hotflash/{patient_id}` | Get hot flash prediction history |
| GET | `/hotflash/{patient_id}/latest` | Get latest hot flash prediction |
| POST | `/hotflash` | Submit hot flash prediction |

### Risk Assessment
| Method | Path | Description |
|--------|------|-------------|
| GET | `/risk/{patient_id}` | Get current risk assessment |
| POST | `/risk` | Update risk assessment |

### Mood Screening
| Method | Path | Description |
|--------|------|-------------|
| GET | `/mood/{patient_id}/screen` | Get latest mood screen result |
| GET | `/mood/{patient_id}/history` | Get mood screen history |
| POST | `/mood/{patient_id}/voice` | Submit voice prosody for screening |

### Bone Health
| Method | Path | Description |
|--------|------|-------------|
| GET | `/bone-risk/{patient_id}` | Get bone risk assessment |

### Triggers
| Method | Path | Description |
|--------|------|-------------|
| GET | `/triggers/{patient_id}` | Get personal trigger analysis (SHAP) |

### Treatment
| Method | Path | Description |
|--------|------|-------------|
| GET | `/treatment/{patient_id}` | Get treatment response tracking |

### Cooling
| Method | Path | Description |
|--------|------|-------------|
| GET | `/cooling/{patient_id}` | Get cooling event history |

### Alerts
| Method | Path | Description |
|--------|------|-------------|
| GET | `/alerts` | List alerts (query: patient_id, severity, limit) |
| POST | `/alerts` | Create alert |
| PUT | `/alerts/{id}/ack` | Acknowledge alert |

### Gynecologist Dashboard
| Method | Path | Description |
|--------|------|-------------|
| GET | `/gynecologists/{id}/patients` | List gynecologist's patients |
| GET | `/gynecologists/{id}/overview` | Patient overview dashboard |

### Reports
| Method | Path | Description |
|--------|------|-------------|
| GET | `/reports/{patient_id}` | Generate clinical PDF report |

### OTA
| Method | Path | Description |
|--------|------|-------------|
| GET | `/ota/check/{node_type}` | Check for firmware updates |

### WebSocket
| Path | Description |
|------|-------------|
| `/ws/realtime/{patient_id}` | Real-time vitals + hot flash + alerts stream |

## Data Models

### Patient
```json
{
  "id": "patient_001",
  "name": "Linda Martinez",
  "age": 52,
  "menopause_stage": "perimenopause",
  "last_period_date": "2026-04-15",
  "gynecologist_id": "gyn_001",
  "bmi": 23.5,
  "treatment": "hrt",
  "treatment_start_date": "2026-06-01",
  "symptoms": ["hot_flashes", "night_sweats", "brain_fog", "insomnia"],
  "family_history_osteoporosis": false
}
```

### Hot Flash Prediction
```json
{
  "patient_id": "patient_001",
  "timestamp": 1754000000,
  "probability": 72,
  "minutes_to_onset": 12,
  "skin_temp_trend": 1,
  "eda_trend": 2,
  "severity_pred": 1,
  "cooling_recommended": true,
  "confidence": 85
}
```

### Risk Assessment
```json
{
  "patient_id": "patient_001",
  "hotflash_risk": 35,
  "nightsweat_risk": 40,
  "sleep_quality": 62,
  "mood_risk": 25,
  "bone_risk": 18,
  "overall_risk": 40,
  "cooling_active": false,
  "alert_level": 1
}
```