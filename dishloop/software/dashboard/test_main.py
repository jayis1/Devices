import unittest
from fastapi.testclient import TestClient
from main import app
class ApiTests(unittest.TestCase):
 def setUp(self):
  import main
  main.events.clear(); main.last_seq.clear()
  self.client=TestClient(app)
 def test_ingest_and_replay_rejected(self):
  import time
  body={'v':1,'node':'n1','seq':1,'kind':'rack_mass_g','ts_ms':int(time.time()*1000),'value':30.0}
  self.assertEqual(self.client.post('/events',json=body).status_code,201)
  self.assertEqual(self.client.post('/events',json=body).status_code,409)
 def test_unknown_kind_rejected(self):
  import time
  body={'v':1,'node':'n1','seq':1,'kind':'unsafe','ts_ms':int(time.time()*1000),'value':1}
  self.assertEqual(self.client.post('/events',json=body).status_code,422)
if __name__=='__main__': unittest.main()
