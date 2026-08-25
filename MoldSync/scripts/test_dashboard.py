from fastapi.testclient import TestClient
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'software', 'dashboard'))
from main import app

client = TestClient(app)
print(client.get('/health').json())
print(client.post('/telemetry/room', json={
    'room_id': 'bathroom-east',
    'air_temp_c': 24.1,
    'rh': 78.2,
    'surface_temp_c': 21.0,
    'co2_ppm': 912,
    'voc_index': 112,
    'moisture_pf': 0.64,
}).json())
print(client.post('/telemetry/plumbing', json={
    'branch_id': 'utility-humidifier',
    'flow_ml_min': 145,
    'leak_signal': 0.81,
    'cold_pipe_c': 9.2,
    'room_temp_c': 18.6,
}).json())
print(client.post('/inspect', json={
    'room_id': 'basement-north',
    'thermal_delta_c': -2.3,
    'conductivity_score': 68,
    'spectral_mildew_index': 0.41,
}).json())
print(client.get('/summary').json())
