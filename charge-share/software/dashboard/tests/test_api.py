import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi.testclient import TestClient
from app import app, latest
class ApiTests(unittest.TestCase):
    def setUp(self):
        latest.clear(); self.c=TestClient(app)
    def test_accept_and_list(self):
        r={"version":1,"node_id":1,"sequence":2,"uptime_ms":1,"millivolts":5000,"milliamps":0,"observed_at":"2026-01-01T00:00:00Z"}
        self.assertEqual(self.c.post('/telemetry',json=r).status_code,200)
        self.assertEqual(self.c.get('/status').json()[0]['node_id'],1)
    def test_reject_replay(self):
        r={"version":1,"node_id":1,"sequence":2,"uptime_ms":1,"millivolts":5000,"milliamps":0,"observed_at":"2026-01-01T00:00:00Z"}
        self.c.post('/telemetry',json=r); self.assertEqual(self.c.post('/telemetry',json=r).status_code,409)
if __name__=='__main__': unittest.main()
