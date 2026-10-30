import unittest
from fastapi.testclient import TestClient
from software.dashboard.app import app
class ApiTests(unittest.TestCase):
    def test_bounds_and_roundtrip(self):
        c=TestClient(app)
        r=c.post('/observations',json={'bed_id':2,'kind':'bloom','value':'category=2','observed_at':1})
        self.assertEqual(r.status_code,201)
        self.assertEqual(c.get('/observations?limit=501').status_code,400)
        self.assertEqual(c.get('/observations').status_code,200)
if __name__=='__main__': unittest.main()
