from fastapi.testclient import TestClient
from main import app

client = TestClient(app)
print(client.get('/health').json())
print(client.post('/telemetry', json={
    'user_id': 'demo-user',
    'node': 'temp-patch',
    'ts': '2026-08-23T07:30:00Z',
    'metrics': {'skin_temp_c': 36.8, 'resting_hr': 64, 'hrv_rmssd': 39.5, 'leak_score': 22}
}).json())
print(client.post('/symptoms', json={
    'user_id': 'demo-user',
    'ts': '2026-08-23T07:45:00Z',
    'pain_score': 4,
    'flow_score': 5,
    'mood_score': 6,
    'notes': 'mild cramps',
    'interventions': ['hydration']
}).json())
print(client.get('/users/demo-user/risk').json())
