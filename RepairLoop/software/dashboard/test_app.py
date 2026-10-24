from fastapi.testclient import TestClient
from app import app

def test_health():
    assert TestClient(app).get('/health').json() == {'ok': True}

def test_reading():
    c=TestClient(app)
    r=c.post('/v1/readings', json={'version':1,'node_type':2,'node_id':4,'sequence':1,'metric':'occupancy','value':1,'unit':'bool'})
    assert r.status_code == 200
    assert c.get('/v1/nodes').json()[0]['node_id'] == 4
