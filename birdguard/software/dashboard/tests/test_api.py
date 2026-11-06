import unittest
from fastapi.testclient import TestClient
import app
class ApiTests(unittest.TestCase):
 def setUp(self):
  app.DB=__import__('pathlib').Path('/tmp/birdguard-test.sqlite3')
  app.DB.unlink(missing_ok=True); self.c=TestClient(app.app)
 def test_ingest_and_status(self):
  r=self.c.post('/v1/observations',json={'version':1,'device_id':'test','seq':1,'ts':'2026-06-01T12:00:00Z','kind':'window','values':{'panel_present':True,'lux':10}})
  self.assertEqual(r.status_code,200); self.assertEqual(len(self.c.get('/v1/status').json()['observations']),1)
 def test_invalid_lux(self):
  r=self.c.post('/v1/observations',json={'version':1,'device_id':'test','seq':1,'ts':'2026-06-01T12:00:00Z','kind':'window','values':{'panel_present':True,'lux':-1}})
  self.assertEqual(r.status_code,422)
 def test_duplicate_rejected(self):
  p={'version':1,'device_id':'test','seq':1,'ts':'2026-06-01T12:00:00Z','kind':'window','values':{'panel_present':True,'lux':10}}
  self.c.post('/v1/observations',json=p); self.assertEqual(self.c.post('/v1/observations',json=p).status_code,409)
if __name__=='__main__': unittest.main()
