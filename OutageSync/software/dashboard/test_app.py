from fastapi.testclient import TestClient
from main import app


def test_health_and_overview() -> None:
    with TestClient(app) as client:
        health = client.get('/api/v1/health')
        assert health.status_code == 200
        overview = client.get('/api/v1/overview')
        assert overview.status_code == 200
        payload = overview.json()
        assert 'forecast' in payload
        assert 'decisions' in payload
        assert payload['decisions']
