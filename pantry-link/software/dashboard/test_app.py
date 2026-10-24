from fastapi.testclient import TestClient
from app import app, records

client = TestClient(app)

def setup_function():
    records.clear()

def test_health():
    assert client.get("/health").json() == {"ok": True}

def test_ingest_duplicate_and_bad_range():
    sample = {"site_id":"demo","node_id":"n1","sequence":1,"kind":"temperature","value":4.2,"unit":"C","timestamp":"2026-01-01T10:00:00Z"}
    assert client.post("/v1/telemetry", json=sample).status_code == 202
    assert client.post("/v1/telemetry", json=sample).status_code == 409
    sample["sequence"] = 2
    sample["value"] = 200
    assert client.post("/v1/telemetry", json=sample).status_code == 422
