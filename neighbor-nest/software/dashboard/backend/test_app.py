from fastapi.testclient import TestClient
from app import app
client = TestClient(app)
assert client.get('/health').status_code == 200
assert client.post('/observations', json={'node_id':'probe-1','kind':'soil','value':42.0,'unit':'percent','observed_at':'2026-11-01T00:00:00Z'}).status_code == 202
assert len(client.get('/observations').json()) == 1
assert client.post('/observations', json={'node_id':'probe-1','kind':'bogus','observed_at':'now'}).status_code == 422
assert client.delete('/observations').status_code == 200
assert client.get('/observations').json() == []
print('API smoke tests passed')
