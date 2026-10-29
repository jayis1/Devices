import unittest
from fastapi.testclient import TestClient
from app import app, latest

class IngestTests(unittest.TestCase):
    def setUp(self):
        latest.clear()
        self.client = TestClient(app)

    def test_ingest_and_duplicate_rejection(self):
        body = {"version": 1, "node_id": "soil-01", "seq": 1, "timestamp": "2026-06-01T12:00:00Z", "kind": "soil", "values": {"soil_proxy_pct": 42.0}}
        self.assertEqual(self.client.post('/v1/telemetry', json=body).status_code, 200)
        self.assertEqual(self.client.post('/v1/telemetry', json=body).status_code, 409)
        self.assertEqual(len(self.client.get('/v1/nodes').json()['nodes']), 1)

if __name__ == '__main__':
    unittest.main()
