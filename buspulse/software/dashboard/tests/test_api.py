from fastapi.testclient import TestClient
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from main import app
client=TestClient(app)
def test_health():
    assert client.get('/health').json()=={'status':'ok'}
def test_telemetry():
    payload={'node_id':1,'sequence':2,'timestamp':123,'value_mv':3300,'temperature_centi_c':2100,'valid':True}
    assert client.post('/telemetry',json=payload).status_code==200
    assert client.get('/nodes').json()['nodes'][0]['node_id']==1
def test_invalid_node():
    payload={'node_id':0,'sequence':2,'timestamp':123,'value_mv':3300,'temperature_centi_c':2100,'valid':True}
    assert client.post('/telemetry',json=payload).status_code==422
