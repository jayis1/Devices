import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi.testclient import TestClient
from app import app

class ApiTests(unittest.TestCase):
    def setUp(self): self.client = TestClient(app)
    def test_ingest_and_delete(self):
        r = self.client.post('/v1/events', json={'v':1,'id':'room-1','seq':3,'t':0,'kind':'level','db10':420})
        self.assertEqual(r.status_code, 200)
        self.assertEqual(len(self.client.get('/v1/events').json()), 1)
        self.assertTrue(self.client.delete('/v1/events').json()['deleted'])
    def test_reject_out_of_range(self):
        r = self.client.post('/v1/events', json={'v':1,'id':'room-1','seq':0,'t':0,'kind':'level','db10':9999})
        self.assertEqual(r.status_code, 422)

if __name__ == '__main__': unittest.main()
