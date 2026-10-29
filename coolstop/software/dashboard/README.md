# CoolStop backend

Python 3.10+. FastAPI with local SQLite telemetry storage. MQTT subscriber is optional; API is unauthenticated prototype software and must remain on a trusted isolated LAN. Configure `COOLSTOP_DB` and `MQTT_HOST` via environment; no secrets are committed.

Run: `python3 -m pip install -r requirements.txt`, then `uvicorn app:app --app-dir .` from this directory. Tests: from repository root, `python3 -m unittest discover -s software/dashboard/tests`.
