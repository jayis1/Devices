from __future__ import annotations

import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path

from fastapi.testclient import TestClient

tmp = tempfile.NamedTemporaryFile(delete=False)
tmp.close()
os.environ['COMMUTESYNC_DB'] = tmp.name

from main import app  # noqa: E402


def test_health_and_overview() -> None:
    with TestClient(app) as client:
        health = client.get('/api/v1/health')
        assert health.status_code == 200
        overview = client.get('/api/v1/overview')
        assert overview.status_code == 200
        payload = overview.json()
        assert 'readiness_risk' in payload
        assert 'lateness_risk' in payload
        assert isinstance(payload['recommended_actions'], list)


def test_ingest_and_node_listing() -> None:
    with TestClient(app) as client:
        entry_payload = {
            'node_id': 'entry-2',
            'required_items': 6,
            'confirmed_items': 5,
            'bag_present': True,
            'badge_seen': True,
            'keys_seen': True,
            'departure_in_minutes': 8,
            'ts': datetime.now(timezone.utc).isoformat(),
        }
        assert client.post('/api/v1/telemetry/entry', json=entry_payload).status_code == 200
        bag_payload = {
            'node_id': 'bag-2',
            'tamper_score': 0.12,
            'separation_m': 0.4,
            'motion_state': 'idle',
            'battery_mv': 2890,
            'ts': datetime.now(timezone.utc).isoformat(),
        }
        assert client.post('/api/v1/telemetry/bag', json=bag_payload).status_code == 200
        assert client.post('/api/v1/actions/checkin', json={'event': 'route_changed', 'note': 'switched line'}).status_code == 200
        nodes = client.get('/api/v1/nodes').json()
        assert 'entry-2' in nodes['entry_nodes']
        assert 'bag-2' in nodes['bag_nodes']


def test_db_file_exists() -> None:
    assert Path(os.environ['COMMUTESYNC_DB']).exists()
