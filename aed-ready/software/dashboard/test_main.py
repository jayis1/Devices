import unittest
from fastapi.testclient import TestClient
from main import app

class ApiTests(unittest.TestCase):
    def setUp(self): self.client = TestClient(app)
    def test_health(self): self.assertEqual(self.client.get('/health').json()['status'], 'ok')
    def test_ingest(self):
        body = dict(node_id=7, sequence=1, received_uptime_s=3, temperature_centi_c=2200, battery_mv=2900, flags=2, status='no_issue_reported')
        self.assertTrue(self.client.post('/v1/cabinets/7/status', json=body).json()['accepted'])
    def test_mismatch(self):
        body = dict(node_id=7, sequence=1, received_uptime_s=3, temperature_centi_c=2200, battery_mv=2900, flags=2, status='unknown')
        self.assertFalse(self.client.post('/v1/cabinets/8/status', json=body).json()['accepted'])

if __name__ == '__main__': unittest.main()
