import unittest
from fastapi.testclient import TestClient
from main import app

class ApiTests(unittest.TestCase):
    def setUp(self):
        self.client = TestClient(app)
    def test_health(self):
        self.assertEqual(self.client.get('/health').status_code, 200)
    def test_accepts_protocol_event(self):
        payload = dict(v=1, node='stall-01', seq=2, ts=0, kind='door', value=0, unit='bool')
        response = self.client.post('/v1/events', json=payload)
        self.assertEqual(response.status_code, 200)
        self.assertTrue(response.json()['accepted'])
    def test_rejects_unknown_version(self):
        payload = dict(v=2, node='stall-01', seq=2, ts=0, kind='door', value=0, unit='bool')
        self.assertEqual(self.client.post('/v1/events', json=payload).status_code, 422)

if __name__ == '__main__':
    unittest.main()
