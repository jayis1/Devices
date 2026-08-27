from fastapi.testclient import TestClient
from main import app, STORE

client = TestClient(app)

def test_health():
    response = client.get('/api/v1/health')
    assert response.status_code == 200
    assert response.json()['status'] == 'ok'

def test_overview_pipeline():
    STORE['dirt'] = []
    STORE['dock'] = None
    STORE['scans'] = []

    client.post('/api/v1/telemetry/dirt', json={
        'node_id': 'dirt-kitchen-1',
        'room': 'kitchen',
        'dust_index': 64,
        'humidity_pct': 68.0,
        'temp_c': 24.3,
        'traffic_score': 77,
        'wet_floor_probability': 0.74,
        'odor_index': 12,
        'battery_mv': 3004
    })
    client.post('/api/v1/dock/state', json={
        'node_id': 'dock-1',
        'clean_tank_ml': 1300,
        'dirty_tank_ml': 110,
        'detergent_ml': 95,
        'leak_detected': False,
        'uv_cycle_ready': True,
        'current_ma': 610
    })
    client.post('/api/v1/wand/scan', json={
        'node_id': 'wand-1',
        'room': 'kitchen',
        'surface': 'counter',
        'residue_class': 'grease',
        'confidence': 0.91,
        'fluorescence_score': 72,
        'image_tag': 'scan-001'
    })

    overview = client.get('/api/v1/overview')
    assert overview.status_code == 200
    data = overview.json()
    assert data['cleanliness_score'] <= 100
    assert any('Wet-floor risk' in alert for alert in data['active_alerts'])
    assert len(data['recommendations']) >= 1
