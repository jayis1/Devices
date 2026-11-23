#!/usr/bin/env python3
"""Local dev helper: validate that the prototype API imports."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
try:
    from fastapi.testclient import TestClient
    from software.dashboard.app import app
except ImportError as exc:
    print(f"Install software/dashboard/requirements.txt first: {exc}", file=sys.stderr)
    raise SystemExit(2)
client = TestClient(app)
assert client.get('/v1/sites/demo').status_code == 200
sample = {"v": 1, "node": "lend-01", "seq": 1, "kind": "slots", "free": 2, "total": 4, "uptime_s": 5}
assert client.post('/v1/readings', json=sample).json() == {"accepted": True}
assert client.post('/v1/readings', json=sample).json()["accepted"] is False
bad = {**sample, "seq": 2, "free": 5}
assert client.post('/v1/readings', json=bad).status_code == 422
assert len(client.get('/v1/sites/demo').json()["docks"]) == 1
print('API import and empty-site smoke test passed')
