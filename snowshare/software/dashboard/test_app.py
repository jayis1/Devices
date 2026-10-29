import unittest
import tempfile
from pathlib import Path
import app
from fastapi.testclient import TestClient

class ApiTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        app.DB = Path(self.temp.name) / 'test.sqlite3'

    def tearDown(self):
        self.temp.cleanup()

    def test_ingest_and_reject_duplicate(self):
        client = TestClient(app.app)
        data = {"version":1,"node_id":"test-node","seq":1,"timestamp":"2026-01-01T00:00:00Z","kind":"ramp","values":{"temp_c":-1.0}}
        self.assertEqual(client.post('/v1/telemetry', json=data).status_code, 200)
        self.assertEqual(client.post('/v1/telemetry', json=data).status_code, 409)
        self.assertEqual(client.get('/v1/telemetry/test-node').json()['seq'], 1)
