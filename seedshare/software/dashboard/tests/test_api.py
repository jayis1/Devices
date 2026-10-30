import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi.testclient import TestClient
from app import app

client = TestClient(app)
def test_health():
    assert client.get('/health').json() == {'ok': True}

def test_event_and_duplicate(tmp_path, monkeypatch):
    import app as module
    monkeypatch.setattr(module, 'DB', str(tmp_path / 'db.sqlite'))
    body = {'version':1,'node_id':'storage-1','sequence':1,'kind':'sample','ts_utc':None,'values':{'rh_pct':52.0}}
    assert client.post('/api/v1/events', json=body).status_code == 200
    assert client.post('/api/v1/events', json=body).json()['duplicate'] is True
    assert client.get('/api/v1/events').json()[0]['node_id'] == 'storage-1'

def test_reject_unknown_field():
    body = {'version':1,'node_id':'n','sequence':0,'kind':'sample','values':{},'surprise':1}
    assert client.post('/api/v1/events', json=body).status_code == 422
