import unittest
from fastapi.testclient import TestClient
from main import app,_latest
class APITest(unittest.TestCase):
 def setUp(self): _latest.clear(); self.c=TestClient(app)
 def test_ingest_and_status(self):
  body={'v':1,'node_id':'weather-1','seq':1,'kind':'rain_tips','value':2,'unit':'tips'}
  self.assertEqual(self.c.post('/v1/telemetry',json=body).status_code,200)
  self.assertEqual(self.c.get('/v1/status').json()['nodes'][0]['value'],2)
 def test_reject_old_sequence(self):
  body={'v':1,'node_id':'n','seq':2,'kind':'wind_mm_s','value':30,'unit':'mm/s'}
  self.c.post('/v1/telemetry',json=body); self.assertEqual(self.c.post('/v1/telemetry',json={**body,'seq':1}).status_code,409)
if __name__=='__main__': unittest.main()
