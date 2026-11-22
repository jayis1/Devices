import unittest
from fastapi.testclient import TestClient
from main import app, _events

class ApiTest(unittest.TestCase):
    def setUp(self):
        _events.clear()
        self.client = TestClient(app)

    def test_idempotent_ingest(self):
        event = {"v":1,"node":"evse-01","seq":2,"ts":10,"kind":"session","state":"started","energy_wh":0}
        self.assertEqual(self.client.post('/events', json=event).status_code, 202)
        self.assertTrue(self.client.post('/events', json=event).json()['duplicate'])
        self.assertEqual(len(self.client.get('/events').json()), 1)

    def test_bounds_and_unknown_kind(self):
        self.assertEqual(self.client.get('/events?limit=999').status_code, 422)
        event = {"v":1,"node":"n","seq":1,"ts":1,"kind":"command"}
        self.assertFalse(self.client.post('/events', json=event).json()['accepted'])

if __name__ == '__main__':
    unittest.main()
