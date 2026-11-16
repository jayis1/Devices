from fastapi.testclient import TestClient
from main import app
c=TestClient(app)
def test_health(): assert c.get('/health').json()['mode']=='local-prototype'
def test_event_and_duplicate():
    e={'v':1,'node':'dock1','seq':5,'ts':100,'kind':'session_start','book':'abc'}
    assert c.post('/events',json=e).status_code==202
    assert c.post('/events',json=e).json()['reason']=='duplicate'
def test_invalid_version():
    assert c.post('/events',json={'v':2,'node':'a','seq':0,'ts':1,'kind':'session_start'}).status_code==422
