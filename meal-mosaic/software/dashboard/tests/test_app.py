import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi.testclient import TestClient
from app import app
class ApiTest(unittest.TestCase):
 def setUp(self): self.client=TestClient(app)
 def test_insert_and_duplicate(self):
  r=self.client.post('/observations',json={'node_id':'test-serving','kind':1,'sequence':712345,'value':1250,'status':0})
  self.assertEqual(r.status_code,200)
  self.assertEqual(self.client.post('/observations',json={'node_id':'test-serving','kind':1,'sequence':712345,'value':1250,'status':0}).status_code,409)
 def test_reject_kind(self): self.assertEqual(self.client.post('/observations',json={'node_id':'x','kind':9,'sequence':0,'value':0,'status':0}).status_code,422)
if __name__=='__main__': unittest.main()
