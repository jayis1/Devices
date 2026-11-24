import unittest
import sys
from pathlib import Path
from fastapi.testclient import TestClient
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from main import app

class ApiTests(unittest.TestCase):
    def setUp(self): self.client = TestClient(app)
    def test_accept_and_dedupe(self):
        event = {"version": 1, "event_id": "abc", "kind": "inspection_done"}
        self.assertEqual(self.client.post("/events", json=event).status_code, 202)
        self.assertTrue(self.client.post("/events", json=event).json()["duplicate"])
        self.assertEqual(len(self.client.get("/events").json()), 1)
    def test_reject_wrong_version(self):
        self.assertEqual(self.client.post("/events", json={"version": 2, "event_id": "x"}).status_code, 422)
    def test_reject_oversized(self):
        self.assertEqual(self.client.post("/events", content=b" " * 385).status_code, 413)
