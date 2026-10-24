import unittest
from datetime import datetime, timezone
from fastapi.testclient import TestClient
from app import app, records

class ApiTest(unittest.TestCase):
    def setUp(self):
        records.clear()
        self.client = TestClient(app)
    def test_health_and_ingest_and_status(self):
        self.assertEqual(self.client.get('/health').status_code, 200)
        body = dict(site_id='demo', node_id=1, sequence=4, received_at=datetime.now(timezone.utc).isoformat(), occupied_mask=3, valid_mask=15, kind=1)
        self.assertEqual(self.client.post('/v1/telemetry', json=body).status_code, 202)
        self.assertEqual(self.client.post('/v1/telemetry', json=body).status_code, 409)
        self.assertEqual(self.client.get('/v1/sites/demo/status').json()['known_seats'], 4)
    def test_invalid_mask_rejected(self):
        body = dict(site_id='demo', node_id=1, sequence=4, received_at=datetime.now(timezone.utc).isoformat(), occupied_mask=2, valid_mask=1, kind=1)
        self.assertEqual(self.client.post('/v1/telemetry', json=body).status_code, 422)

if __name__ == '__main__':
    unittest.main()
