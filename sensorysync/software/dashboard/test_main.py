# Authored by jayis1.
from datetime import datetime, timezone
from main import Command, Telemetry, cards, command, health, telemetry
def test_telemetry_model():
    item=Telemetry(node_id=16,epoch=1,seq=2,quality="ok",metrics={"lux":100.0})
    assert item.metrics["lux"] == 100.0

def test_api_vertical_slice():
    assert health()["status"] == "ok"
    token = "Bearer change-before-deploy"
    event = Telemetry(node_id=16, epoch=1, seq=99, quality="stale", metrics={"lux": 100.0})
    assert telemetry(event, token)["accepted"] is True
    assert len(cards(token)) == 1
    request = Command(node_id=48, kind="set_light_level", value=0.2, expires_at=int(datetime.now(timezone.utc).timestamp()) + 10)
    assert command(request, token)["accepted"] is True

if __name__ == "__main__":
    test_telemetry_model()
    test_api_vertical_slice()
    print("dashboard model smoke test passed")
