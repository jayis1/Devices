import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi.testclient import TestClient
from app import app, latest
class ApiTests(unittest.TestCase):
 def setUp(self): latest.clear(); self.client=TestClient(app)
 def test_ingest_and_status(self):
  r=self.client.post('/v1/telemetry',json={'node_id':'wheel1','sequence':1,'kind':'wheel_deviation','value':3.0,'unit':'mg'})
  self.assertEqual(r.status_code,200); self.assertEqual(self.client.get('/v1/status').json()['readings'][0]['value'],3.0)
 def test_stale_and_invalid(self):
  payload={'node_id':'wheel1','sequence':1,'kind':'pressure','value':200,'unit':'kPa'}
  self.assertEqual(self.client.post('/v1/telemetry',json=payload).status_code,200)
  self.assertEqual(self.client.post('/v1/telemetry',json=payload).status_code,409)
  payload['sequence']=2;payload['kind']='unknown'
  self.assertEqual(self.client.post('/v1/telemetry',json=payload).status_code,422)
if __name__=='__main__': unittest.main()
