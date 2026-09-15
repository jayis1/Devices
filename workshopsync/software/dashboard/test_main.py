# Dashboard integration smoke tests — authored by jayis1.
from fastapi.testclient import TestClient
from main import app

client = TestClient(app)
TOKEN = {"Authorization": "Bearer change-before-deploy"}

def test_health() -> None:
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json()["status"] == "ok"

def test_telemetry_auth_and_idempotency() -> None:
    body = {"node_id": 16, "seq": 1, "epoch": 1, "kind": "tool", "quality": "fault", "metrics": {"vibration": 1.2}}
    assert client.post("/v1/telemetry", json=body).status_code == 401
    assert client.post("/v1/telemetry", headers=TOKEN, json=body).status_code == 202
    assert client.post("/v1/telemetry", headers=TOKEN, json=body).status_code == 409
    cards = client.get("/v1/cards", headers=TOKEN)
    assert cards.status_code == 200 and len(cards.json()) == 1

if __name__ == "__main__":
    test_health()
    test_telemetry_auth_and_idempotency()
    print("WorkshopSync dashboard smoke tests passed")
