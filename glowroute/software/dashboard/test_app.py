from fastapi.testclient import TestClient
from app import app, latest
def test_ingest_and_replay():
    latest.clear(); c=TestClient(app)
    x={"v":1,"node":1,"seq":1,"lux":12.5,"uptime_s":4}
    assert c.post("/telemetry",json=x).status_code==200
    assert c.post("/telemetry",json=x).status_code==409
    assert c.get("/telemetry").json()[0]["lux"]==12.5
