import unittest
from fastapi.testclient import TestClient
from app import app, store
class ApiTests(unittest.TestCase):
 def setUp(self): store.clear(); self.c=TestClient(app)
 def test_health_and_latest(self):
  self.assertEqual(self.c.get('/health').json()['status'],'ok')
  p={'schema_version':1,'node_id':'room-1','sequence':1,'captured_at':'2026-01-01T00:00:00Z','co2_ppm':800}
  self.assertEqual(self.c.post('/v1/observations',json=p).status_code,200)
  self.assertEqual(self.c.get('/v1/sites/site-a/latest').json()['observations'][0]['co2_ppm'],800)
 def test_duplicate_rejected(self):
  p={'schema_version':1,'node_id':'room-1','sequence':1,'captured_at':'2026-01-01T00:00:00Z','co2_ppm':800}
  self.c.post('/v1/observations',json=p); self.assertEqual(self.c.post('/v1/observations',json=p).status_code,409)
