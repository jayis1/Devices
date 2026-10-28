from fastapi.testclient import TestClient
from app import app, latest

client = TestClient(app)
def test_invalid_level_rejected():
    latest.clear()
    r = client.post("/telemetry", json={"version":1,"node_id":1,"sequence":1,"epoch_minutes":int(__import__('time').time()//60),"leq_cdb":15000,"peak_cdb":15000,"flags":0})
    assert r.status_code == 422

def test_health():
    assert client.get("/health").json()["ok"] is True
