from fastapi.testclient import TestClient
from main import app


def test_health_overview_and_command() -> None:
    with TestClient(app) as client:
        assert client.get('/api/v1/health').status_code == 200
        overview = client.get('/api/v1/overview')
        assert overview.status_code == 200
        payload = overview.json()
        assert 'backup_forecast' in payload
        assert payload['top_clog_risks']
        command = client.post('/api/v1/commands/valve', json={'command': 'close', 'reason': 'storm test'})
        assert command.status_code == 200
        assert command.json()['accepted'] is True
