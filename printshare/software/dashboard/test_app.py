from fastapi.testclient import TestClient
from app import app

client = TestClient(app)

def test_health():
    assert client.get("/health").json() == {"ok": True}

def test_telemetry_and_validation():
    sample = dict(version=1, type=1, node_id=7, sequence=2, uptime_s=60, state=2,
                  humidity_centi_rh=0, temperature_centi_c=0, battery_mv=3300)
    assert client.post("/telemetry", json=sample).status_code == 200
    sample["humidity_centi_rh"] = 10001
    assert client.post("/telemetry", json=sample).status_code == 422
