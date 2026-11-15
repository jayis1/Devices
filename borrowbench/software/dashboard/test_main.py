import unittest, tempfile, os
from fastapi.testclient import TestClient
import main
class ApiTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(); main.DB=os.path.join(self.tmp.name,'test.db'); self.c=TestClient(main.app)
 def tearDown(self): self.tmp.cleanup()
 def test_insert_and_duplicate(self):
  e={'v':1,'id':'dock-a','seq':3,'kind':'presence','ts_ms':12,'payload':{'present':True}}
  self.assertEqual(self.c.post('/events',json=e).status_code,200)
  self.assertTrue(self.c.post('/events',json=e).json()['duplicate'])
  self.assertEqual(len(self.c.get('/events').json()),1)
 def test_oversized(self):
  e={'v':1,'id':'x','seq':1,'kind':'weight','ts_ms':2,'payload':{'x':'a'*600}}
  self.assertEqual(self.c.post('/events',json=e).status_code,413)
if __name__=='__main__': unittest.main()
