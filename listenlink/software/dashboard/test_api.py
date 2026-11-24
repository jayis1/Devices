from fastapi.testclient import TestClient
from main import app

def test_ingest_and_replay():
 c=TestClient(app); payload={'site':'test','room':'hall','sequence':1,'transmitter_on':True,'receiver_count':2,'observed_at':'2026-11-24T12:00:00Z'}
 assert c.post('/v1/readings',json=payload).status_code==200
 assert c.post('/v1/readings',json=payload).status_code==409
 assert c.get('/v1/readiness').status_code==200

def test_bounds():
 c=TestClient(app); payload={'site':'test','room':'x','sequence':2,'transmitter_on':True,'receiver_count':9,'observed_at':'2026-11-24T12:00:00Z'}
 assert c.post('/v1/readings',json=payload).status_code==422
