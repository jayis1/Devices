from fastapi.testclient import TestClient

from main import app

client = TestClient(app)

def test_health() -> None:
    response = client.get('/health')
    assert response.status_code == 200
    assert response.json()['status'] == 'ok'

def test_transfer_and_summary() -> None:
    r1 = client.post('/telemetry/transfer', json={
        'zone_id': 'bedroom-chair',
        'asymmetry_pct': 24.0,
        'unload_rate': 8.0,
        'retries': 1,
        'occupied': True,
    })
    assert r1.status_code == 200
    assert r1.json()['level'] in {'amber', 'red'}

    client.post('/telemetry/walker', json={
        'walker_id': 'walker-1',
        'grip_force_n': 40.0,
        'wheel_speed_rps': 4.2,
        'slip_score': 1.1,
        'brake_state': 'released',
    })
    client.post('/telemetry/band', json={
        'wearer_id': 'user-1',
        'hr_bpm': 112,
        'hrv_proxy': 18,
        'fall_confidence': 20,
        'sos': False,
    })
    summary = client.get('/summary')
    assert summary.status_code == 200
    assert summary.json()['command_count'] >= 2
