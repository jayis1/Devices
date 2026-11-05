import unittest
from fastapi.testclient import TestClient
from main import app, latest

class ApiTests(unittest.TestCase):
    def setUp(self):
        latest.clear()
        self.client = TestClient(app)
    def test_accept_and_sequence(self):
        payload = {"v":1,"node_id":"flow-01","seq":1,"ts":int(__import__('time').time()),"kind":"flow","values":{"rate":3.0},"unit":{"rate":"L/min"}}
        self.assertEqual(self.client.post('/telemetry', json=payload).status_code, 200)
        self.assertEqual(self.client.post('/telemetry', json=payload).status_code, 409)
    def test_bad_value_rejected(self):
        payload = {"v":1,"node_id":"flow-01","seq":1,"ts":int(__import__('time').time()),"kind":"flow","values":{"rate":400},"unit":{"rate":"L/min"}}
        self.assertEqual(self.client.post('/telemetry', json=payload).status_code, 422)

if __name__ == '__main__':
    unittest.main()
