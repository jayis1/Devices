# SeizureSync — Cloud Backend (FastAPI + MQTT + TimescaleDB)

HIPAA-compliant backend for seizure event ingestion, ML inference,
neurologist report generation, and Twilio emergency dispatch.

## Stack
- **FastAPI** — REST + WebSocket server
- **TimescaleDB** — time-series signals + events
- **Mosquitto** — MQTT broker for device ingestion
- **Celery + Redis** — async report generation
- **Twilio** — emergency dispatch
- **MinIO** (S3-compatible) — raw signal storage

## Run

```bash
cd software/dashboard
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8000
```

## Endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/patients` | List patients |
| POST | `/patients` | Register patient |
| GET | `/patients/{id}` | Patient detail |
| GET | `/patients/{id}/events` | Seizure events |
| POST | `/patients/{id}/events` | Manual event entry |
| GET | `/patients/{id}/diary` | Seizure diary |
| GET | `/patients/{id}/risk` | 24-hr risk forecast |
| GET | `/patients/{id}/sudep` | SUDEP risk score |
| POST | `/patients/{id}/reports` | Generate neurologist report |
| GET | `/patients/{id}/reports/{rid}` | Download report (PDF) |
| GET | `/patients/{id}/alerts` | Alert history |
| POST | `/webhooks/twilio` | Twilio status callback |
| WS  | `/ws/{id}` | Real-time alert stream |