import unittest, tempfile, os
from fastapi.testclient import TestClient
import main
class ApiTest(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(); main.DB=os.path.join(self.tmp.name,'test.db'); self.c=TestClient(main.app)
 def tearDown(self): self.tmp.cleanup()
 def test_create_list_call_and_delete(self):
  r=self.c.post('/tickets',json={'category':'electronics'}); self.assertEqual(r.status_code,200); tid=r.json()['id']
  self.assertEqual(self.c.get('/tickets').json()[0]['id'],tid)
  self.assertEqual(self.c.post(f'/tickets/{tid}/called').json()['status'],'called')
  self.assertEqual(self.c.post(f'/tickets/{tid}/called').status_code,404)
  self.c.delete('/tickets'); self.assertEqual(self.c.get('/tickets').json(),[])
 def test_reject_bad_category(self): self.assertEqual(self.c.post('/tickets',json={'category':'x'*40}).status_code,422)
if __name__=='__main__': unittest.main()
