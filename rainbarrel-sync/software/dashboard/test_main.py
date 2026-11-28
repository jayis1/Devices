from fastapi.testclient import TestClient
from main import app, latest

def test_ingest_and_status():
    latest.clear()
    c = TestClient(app)
    r = c.post("/api/v1/telemetry", json={"node_id":"tank-1","kind":"tank_distance","value":523,"unit":"mm","sequence":1,"observed_at":"2026-11-28T10:00:00Z"})
    assert r.status_code == 200 and r.json()["accepted"]
    assert c.get("/api/v1/status").json()["nodes"]["tank-1"]["value"] == 523

def test_reject_invalid_kind_and_timezone():
    c = TestClient(app)
    base={"node_id":"tank-1","kind":"arbitrary","value":1,"unit":"mm","sequence":1,"observed_at":"2026-11-28T10:00:00Z"}
    assert c.post("/api/v1/telemetry",json=base).status_code == 422
    base["kind"]="tank_distance"; base["observed_at"]="2026-11-28T10:00:00"
    assert c.post("/api/v1/telemetry",json=base).status_code == 422
