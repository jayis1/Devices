from fastapi.testclient import TestClient
import main


def test_accept_and_duplicate(tmp_path, monkeypatch):
    monkeypatch.setattr(main, "DB_PATH", str(tmp_path / "events.db"))
    main.initialize()
    client = TestClient(main.app)
    body = dict(version=1, node_id="tray_1", sequence=1, kind="staff_intake", value=120, monotonic_ms=42)
    assert client.post("/events", json=body).status_code == 202
    assert client.post("/events", json=body).status_code == 409


def test_reject_invalid_values(tmp_path, monkeypatch):
    monkeypatch.setattr(main, "DB_PATH", str(tmp_path / "events.db"))
    main.initialize()
    client = TestClient(main.app)
    body = dict(version=1, node_id="shelf", sequence=2, kind="shelf_door", value=2, monotonic_ms=1)
    assert client.post("/events", json=body).status_code == 422


def test_reject_unknown_kind(tmp_path, monkeypatch):
    monkeypatch.setattr(main, "DB_PATH", str(tmp_path / "events.db"))
    main.initialize()
    client = TestClient(main.app)
    body = dict(version=1, node_id="shelf", sequence=3, kind="command", value=1, monotonic_ms=1)
    assert client.post("/events", json=body).status_code == 422
