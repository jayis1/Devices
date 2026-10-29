import tempfile
import unittest
from pathlib import Path
import sys

from fastapi.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import app


class ApiTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        app.DB_PATH = Path(self.temp.name) / "test.sqlite3"
        self.client = TestClient(app.app)

    def tearDown(self):
        self.temp.cleanup()

    def test_accept_and_read(self):
        payload = {"version": 1, "node_id": "climate-1", "seq": 2,
                   "timestamp": "2026-06-20T14:05:00Z", "kind": "climate",
                   "values": {"temperature_c": 29.4, "relative_humidity_pct": 38.2}}
        self.assertEqual(self.client.post("/telemetry", json=payload).status_code, 200)
        self.assertEqual(self.client.get("/telemetry/latest").json()[0]["node_id"], "climate-1")

    def test_reject_bad_version_range_and_extra(self):
        payload = {"version": 9, "node_id": "n", "seq": 0, "timestamp": "2026-06-20T14:05:00Z",
                   "kind": "supply", "values": {"mass_g": -1}}
        self.assertEqual(self.client.post("/telemetry", json=payload).status_code, 422)
        payload["version"] = 1
        self.assertEqual(self.client.post("/telemetry", json=payload).status_code, 422)
        payload["values"] = {"mass_g": 1}
        payload["extra"] = "reject"
        self.assertEqual(self.client.post("/telemetry", json=payload).status_code, 422)


if __name__ == "__main__":
    unittest.main()
