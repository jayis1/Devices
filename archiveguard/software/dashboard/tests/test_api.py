import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi.testclient import TestClient
from app import app
class IngestTests(unittest.TestCase):
    def test_ingest_and_duplicate(self):
        client = TestClient(app)
        body = {"schema_version":1,"site_id":"test-site","node_id":"room-1","seq":9101,"captured_at":"2026-01-01T00:00:00Z","temperature_c":20,"relative_humidity_pct":45}
        self.assertTrue(client.post('/v1/telemetry', json=body).json()['accepted'])
        self.assertTrue(client.post('/v1/telemetry', json=body).json()['duplicate'])
    def test_reject_range(self):
        client = TestClient(app)
        body = {"schema_version":1,"site_id":"test-site","node_id":"room-1","seq":9102,"captured_at":"2026-01-01T00:00:00Z","temperature_c":99,"relative_humidity_pct":45}
        self.assertEqual(client.post('/v1/telemetry', json=body).status_code, 422)
if __name__ == '__main__': unittest.main()
