import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from fastapi.testclient import TestClient
from main import app

client = TestClient(app)
def test_health_and_validation():
    assert client.get('/health').json()['status'] == 'ok'
    payload = {'version': 1, 'node_id': 'landing-a', 'sequence': 1,
               'status': 'clear', 'created_at': '2026-01-01T00:00:00Z'}
    assert client.post('/events', json=payload).status_code == 200
    assert client.post('/events', json=payload).status_code == 409
    payload['node_id'] = 'intruder'
    payload['sequence'] = 2
    assert client.post('/events', json=payload).status_code == 403
    assert client.post('/events', json={}).status_code == 422
