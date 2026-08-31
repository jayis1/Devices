from __future__ import annotations

import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path

from fastapi.testclient import TestClient

tmp = tempfile.NamedTemporaryFile(delete=False)
tmp.close()
os.environ['PREGNANCYSYNC_DB'] = tmp.name

from main import app  # noqa: E402


def test_health_and_overview() -> None:
    with TestClient(app) as client:
        health = client.get('/api/v1/health')
        assert health.status_code == 200
        overview = client.get('/api/v1/overview')
        assert overview.status_code == 200
        payload = overview.json()
        assert 'reduced_movement_risk' in payload
        assert 'hypertensive_risk' in payload
        assert isinstance(payload['recommended_actions'], list)


def test_ingest_band_and_checkin() -> None:
    with TestClient(app) as client:
        band_payload = {
            'node_id': 'band-2',
            'movement_count_10m': 9,
            'movement_variability': 0.51,
            'posture_pct_left': 48,
            'posture_pct_supine': 22,
            'skin_temp_c': 34.8,
            'ehg_activity_index': 7.5,
            'battery_mv': 3860,
            'ts': datetime.now(timezone.utc).isoformat(),
        }
        resp = client.post('/api/v1/telemetry/band', json=band_payload)
        assert resp.status_code == 200
        checkin = client.post('/api/v1/actions/checkin', json={'symptom': 'headache', 'severity': 2, 'note': 'mild'})
        assert checkin.status_code == 200
        nodes = client.get('/api/v1/nodes').json()
        assert 'band-2' in nodes['band_nodes']


def test_script_outputs_are_local_files() -> None:
    assert Path(os.environ['PREGNANCYSYNC_DB']).exists()
