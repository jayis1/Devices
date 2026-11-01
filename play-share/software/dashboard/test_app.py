import unittest
from fastapi.testclient import TestClient
from app import app, events

class ApiTests(unittest.TestCase):
    def setUp(self):
        events.clear()
        self.client = TestClient(app)
    def test_valid_then_duplicate(self):
        payload = {"v":1,"id":"rack-1","node":"rack","seq":1,"ts_ms":10,"kind":"stock","value":3,"unit":"items"}
        self.assertTrue(self.client.post("/events", json=payload).json()["accepted"])
        self.assertFalse(self.client.post("/events", json=payload).json()["accepted"])
    def test_invalid_kind_and_malformed(self):
        payload = {"v":1,"id":"rack-1","node":"rack","seq":1,"ts_ms":10,"kind":"inspection","value":3,"unit":"event"}
        self.assertEqual(self.client.post("/events", json=payload).status_code, 422)
        self.assertEqual(self.client.post("/events", content="{").status_code, 422)

if __name__ == "__main__": unittest.main()
