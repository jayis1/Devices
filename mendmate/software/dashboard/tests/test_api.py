import unittest
import sys
from pathlib import Path
from datetime import datetime, timezone
from fastapi.testclient import TestClient
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from main import app, latest, repairs

class ApiTests(unittest.TestCase):
 def setUp(self):
  latest.clear(); repairs.clear(); self.client=TestClient(app)
 def event(self, seq=0):
  return {'schema':'mendmate.telemetry.v1','device_id':'reader-1','boot_id':'boot-a','seq':seq,'observed_at':datetime.now(timezone.utc).isoformat(),'kind':'swatch','payload':{'channels':[123]*11}}
 def test_health_and_ingest(self):
  self.assertEqual(self.client.get('/health').json(),{'status':'ok'})
  self.assertEqual(self.client.post('/v1/events',json=self.event()).status_code,200)
  self.assertEqual(len(self.client.get('/v1/status').json()['nodes']),1)
 def test_duplicate_and_bad_payload(self):
  self.client.post('/v1/events',json=self.event())
  self.assertEqual(self.client.post('/v1/events',json=self.event()).status_code,409)
  bad=self.event(1); bad['payload']['channels']=[1]
  self.assertEqual(self.client.post('/v1/events',json=bad).status_code,422)
 def test_repair_text_limits(self):
  self.assertEqual(self.client.post('/v1/repairs',json={'garment':'shirt','damage':'loose hem'}).status_code,200)
  self.assertEqual(self.client.post('/v1/repairs',json={'garment':'x'*101,'damage':'tear'}).status_code,422)
if __name__=='__main__': unittest.main()
