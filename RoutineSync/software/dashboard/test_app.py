from fastapi.testclient import TestClient

from main import app


client = TestClient(app)


def test_health() -> None:
    response = client.get('/api/v1/health')
    assert response.status_code == 200
    assert response.json()['status'] == 'ok'


def test_departure_evaluate() -> None:
    response = client.post(
        '/api/v1/departure/evaluate',
        json={
            'routine': 'workday',
            'missing_items': 1,
            'tray_mass_delta': 142.0,
            'door_open': True,
            'minutes_to_deadline': 9,
            'focus_fragmentation': 0.3,
        },
    )
    assert response.status_code == 200
    body = response.json()
    assert body['miss_risk'] > 0.5
    assert body['nudge'] in {'tag_chirp', 'epaper_prompt', 'led_shift', 'none'}


def test_find_item() -> None:
    response = client.post(
        '/api/v1/find-item',
        json={
            'item_name': 'keys',
            'last_seen_room': 'office',
            'minutes_since_seen': 12,
            'movement_events': 0,
            'doorway_seen': False,
        },
    )
    assert response.status_code == 200
    assert response.json()['best_room'] == 'office'
