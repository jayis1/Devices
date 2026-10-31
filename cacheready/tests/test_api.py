import tempfile
import unittest
from datetime import datetime, timezone
from fastapi.testclient import TestClient
from cacheready.software.dashboard import app as service

class ApiTests(unittest.TestCase):
    def test_health_and_idempotent_ingest(self):
        with tempfile.TemporaryDirectory() as tmp:
            service.DB = f"{tmp}/test.sqlite3"
            with TestClient(service.app) as client:
                self.assertEqual(client.get("/health").status_code, 200)
                observation = {"node_id": 1, "sequence": 7, "observed_at": datetime.now(timezone.utc).isoformat(), "temperature_c": 21.5, "door_open": False, "supply_v": None}
                self.assertTrue(client.post("/api/v1/observations", json=observation).json()["accepted"])
                self.assertFalse(client.post("/api/v1/observations", json=observation).json()["accepted"])
                bad = dict(observation, temperature_c=300)
                self.assertEqual(client.post("/api/v1/observations", json=bad).status_code, 422)

if __name__ == "__main__":
    unittest.main()
