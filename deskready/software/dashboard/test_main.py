from fastapi.testclient import TestClient
from main import app,state
c=TestClient(app)
def setup_function(): state.clear()
def test_health(): assert c.get('/health').json()=={'ok':True}
def test_ingest_and_duplicate():
 e={'v':1,'node':'desk-01','seq':1,'kind':'checkin','ready':True,'uptime_s':3}
 assert c.post('/events',json=e).status_code==200
 assert c.get('/stations').json()['desk-01']['ready'] is True
 assert c.post('/events',json=e).status_code==409
def test_invalid():
 assert c.post('/events',json={'v':9}).status_code==422
