from fastapi.testclient import TestClient
from main import app

def test_ingest_and_duplicate():
    client = TestClient(app)
    event = {"version": 1, "device_id": 9, "sequence": 1, "kind": "weight", "value_a": 1234}
    assert client.post("/v1/events", json=event).status_code == 202
    assert client.post("/v1/events", json=event).json()["duplicate"] is True

def test_reject_kind():
    client = TestClient(app)
    event = {"version": 1, "device_id": 10, "sequence": 1, "kind": "actuate", "value_a": 0}
    assert client.post("/v1/events", json=event).status_code == 400
