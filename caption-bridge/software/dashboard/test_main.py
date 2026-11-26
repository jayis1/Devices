from fastapi.testclient import TestClient
from main import app, segments

client = TestClient(app)

def setup_function():
    segments.clear()
    import main
    main.last_seq = -1

def test_caption_ingest_and_readback():
    response = client.post("/sessions/room-1/captions", json={"session":"room-1","seq":1,"text":"Welcome","final":True})
    assert response.status_code == 200
    assert client.get("/sessions/room-1/captions").json()["captions"][0]["text"] == "Welcome"

def test_rejects_duplicate_and_wrong_session():
    payload = {"session":"room-1","seq":1,"text":"Hello","final":True}
    assert client.post("/sessions/room-1/captions", json=payload).status_code == 200
    assert client.post("/sessions/room-1/captions", json=payload).status_code == 409
    payload["session"] = "other"
    assert client.post("/sessions/room-1/captions", json=payload).status_code == 422

def test_rejects_oversized_caption():
    payload = {"session":"room-1","seq":1,"text":"x"*2001,"final":True}
    assert client.post("/sessions/room-1/captions", json=payload).status_code == 422
