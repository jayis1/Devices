from fastapi.testclient import TestClient
from parkkeys.software.dashboard.app import app

client = TestClient(app)

def test_event_and_duplicate_are_idempotent():
    payload = {"v": 1, "event_id": "event-1", "node_id": "station-1", "seq": 1,
               "observed_at": "2025-01-01T00:00:00Z", "kind": "piano_report",
               "value": {"status": "needs-cleaning"}}
    assert client.post("/events", json=payload).json() == {"accepted": True, "duplicate": False}
    assert client.post("/events", json=payload).json() == {"accepted": True, "duplicate": True}
    assert len(client.get("/reports").json()) == 1

def test_rejects_unknown_kind():
    payload = {"v": 1, "event_id": "event-2", "node_id": "station-1", "seq": 2,
               "observed_at": "2025-01-01T00:00:00Z", "kind": "launch", "value": {}}
    assert client.post("/events", json=payload).status_code == 422
