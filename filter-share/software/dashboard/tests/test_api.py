import sys
from pathlib import Path
from fastapi.testclient import TestClient
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from main import app
client=TestClient(app)
def test_health():
    assert client.get('/health').json()['ok']
def test_accept_and_duplicate():
    d={'site_id':'demo','node_id':'dock-1','sequence':5,'kind':'dock','value_a':1,'value_b':0}
    assert client.post('/v1/snapshot',json=d).status_code==202
    assert client.post('/v1/snapshot',json=d).status_code==409
def test_range():
    d={'site_id':'demo','node_id':'pm-1','sequence':1,'kind':'pm','value_a':99999,'value_b':0}
    assert client.post('/v1/snapshot',json=d).status_code==422
