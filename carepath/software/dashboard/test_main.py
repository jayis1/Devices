from fastapi.testclient import TestClient
from main import app, latest
client = TestClient(app)
def setup_function(): latest.clear()
def test_unknown_until_both_nodes():
    assert client.get('/status').json()['state'] == 'UNKNOWN'
def test_end_to_end_and_replay_rejected():
    a={'v':1,'node':'entry-1','seq':1,'uptime_ms':10,'kind':'entry','door_open':False,'staff_check':True,'valid':True}
    b={'v':1,'node':'approach-1','seq':1,'uptime_ms':11,'kind':'approach','clearance_mm':1000,'valid':True}
    assert client.post('/observations',json=a).status_code == 200
    assert client.post('/observations',json=b).status_code == 200
    assert client.get('/status').json()['state'] == 'OPERATOR_CHECKED'
    assert client.post('/observations',json=a).status_code == 409
def test_reject_invalid_payload():
    assert client.post('/observations',json={'v':4}).status_code == 422
