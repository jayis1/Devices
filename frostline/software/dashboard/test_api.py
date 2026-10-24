from fastapi.testclient import TestClient
import main
from main import app

def test_health():
    assert TestClient(app).get('/health').json()=={'status':'ok'}

def test_ingest_latest(tmp_path, monkeypatch):
    monkeypatch.setattr(main, 'DB_PATH', str(tmp_path/'test.db'))
    c=TestClient(app)
    body={'site_id':'demo','node_id':1,'sequence':3,'observed_at':'2026-01-01T00:00:00Z','surface_temp_c':-1.2,'wetness':1,'battery_mv':3700}
    assert c.post('/api/v1/telemetry',json=body).status_code==200
    assert c.get('/api/v1/sites/demo/latest').json()['records'][0]['wetness']==1
    assert c.post('/api/v1/telemetry',json=body).status_code==409
