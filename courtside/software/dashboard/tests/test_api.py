import unittest
import sys
from pathlib import Path
from fastapi.testclient import TestClient
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from main import app, records, seen

class ApiTest(unittest.TestCase):
    def setUp(self):
        records.clear(); seen.clear(); self.client = TestClient(app)

    def payload(self):
        return {"version": 1, "site_id": "park-17", "node_id": "surface-a", "seq": 1,
                "observed_at": "2026-05-20T12:00:00Z", "kind": "surface_observation",
                "data": {"wetness_raw": 812}}

    def test_accept_duplicate_and_read(self):
        self.assertEqual(self.client.post('/observations', json=self.payload()).status_code, 200)
        duplicate = self.client.post('/observations', json=self.payload()).json()
        self.assertTrue(duplicate['duplicate'])
        self.assertEqual(len(self.client.get('/observations').json()), 1)

    def test_reject_malformed_and_unknown_fields(self):
        p = self.payload(); p['version'] = 2
        self.assertEqual(self.client.post('/observations', json=p).status_code, 422)
        p = self.payload(); p['unexpected'] = True
        self.assertEqual(self.client.post('/observations', json=p).status_code, 422)

if __name__ == '__main__':
    unittest.main()
