import os, sys, tempfile, unittest
from datetime import datetime, timezone
from pathlib import Path
from fastapi.testclient import TestClient
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
class ApiTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(); os.environ['COOKSHARE_DB']=os.path.join(self.tmp.name,'db.sqlite')
  from app import app
  self.client=TestClient(app)
 def tearDown(self): self.tmp.cleanup(); os.environ.pop('COOKSHARE_DB',None)
 def test_idempotent(self):
  p={'node_id':4,'sequence':1,'state':'idle','sample_mA':20,'observed_at':datetime.now(timezone.utc).isoformat()}
  self.assertEqual(self.client.post('/v1/observations',json=p).status_code,202)
  self.assertEqual(self.client.post('/v1/observations',json=p).json()['status'],'duplicate')
  self.assertEqual(self.client.get('/v1/stations/4').json()['state'],'idle')
 def test_invalid_state(self):
  p={'node_id':1,'sequence':1,'state':'available','observed_at':datetime.now(timezone.utc).isoformat()}
  self.assertEqual(self.client.post('/v1/observations',json=p).status_code,422)
