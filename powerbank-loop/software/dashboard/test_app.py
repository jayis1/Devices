from fastapi.testclient import TestClient
from app import app, latest, last_seq
c=TestClient(app)
def setup_function(): latest.clear(); last_seq.clear()
def event(**kw):
    d={"v":1,"site":"library-a","node":"dock-1","seq":1,"ts":1000,"kind":"DOCK_STATUS","payload":{"occupied":2,"total":4,"ready":1,"faults":0}}
    d.update(kw); return d
def test_ingest_and_duplicate():
    assert c.post('/v1/events',json=event()).status_code==202
    assert c.post('/v1/events',json=event()).json()=={"accepted":False,"reason":"duplicate_or_stale"}
    assert len(c.get('/v1/sites/library-a/latest').json()['events'])==1
def test_reject_bad_schema():
    assert c.post('/v1/events',json=event(kind='REMOTE_COMMAND')).status_code==422
    assert c.post('/v1/events',json=event(payload=[])).status_code==422
    assert c.post('/v1/events',json=event(extra='no')).status_code==422
    assert c.post('/v1/events',json=event(payload={"occupied":8,"total":2,"ready":1,"faults":0})).status_code==422
def test_health(): assert c.get('/health').json()=={"status":"ok"}
def test_pack_payload():
    p={"pack_id":"a1b2c3d4","soc_bucket":3,"temp_deci_c":217}
    assert c.post('/v1/events',json=event(kind='PACK_STATUS',payload=p)).status_code==202
