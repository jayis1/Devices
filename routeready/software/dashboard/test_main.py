import os, tempfile, unittest
from fastapi.testclient import TestClient
import main
class ApiTest(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(); main.DB=os.path.join(self.tmp.name,'test.db'); self.c=TestClient(main.app)
 def tearDown(self): self.tmp.cleanup()
 def payload(self, **kw):
  d={'v':1,'node':'section-a','seq':1,'kind':'section_state','value':'closed','utc_ms':0}; d.update(kw); return d
 def test_accept_dedupe_and_list(self):
  self.assertEqual(self.c.post('/events',json=self.payload()).status_code,200)
  self.assertEqual(self.c.post('/events',json=self.payload()).json()['duplicate'],True)
  self.assertEqual(len(self.c.get('/events').json()),1)
 def test_reject_invalid_enum_and_version(self):
  self.assertEqual(self.c.post('/events',json=self.payload(value='safe')).status_code,422)
  self.assertEqual(self.c.post('/events',json=self.payload(v=2)).status_code,422)
if __name__=='__main__': unittest.main()
