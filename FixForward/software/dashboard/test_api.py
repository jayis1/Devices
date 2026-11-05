import unittest, tempfile, os
from fastapi.testclient import TestClient
class ApiTest(unittest.TestCase):
 def test_contract(self):
  with tempfile.TemporaryDirectory() as d:
   os.environ['FIXFORWARD_DB']=d+'/t.db'
   from app import app,init
   init()
   c=TestClient(app)
   x={'id':'e1','category':'phone','weight_g':200,'route':'repair'}
   self.assertEqual(c.post('/v1/intake',json=x).status_code,200)
   self.assertEqual(c.post('/v1/intake',json=x).status_code,409)
   self.assertEqual(c.get('/v1/events').json()[0]['weight_g'],200)
if __name__=='__main__':unittest.main()
