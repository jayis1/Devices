import json
import tempfile
import unittest
from pathlib import Path
from fastapi.testclient import TestClient
import app

class ApiTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        app.DB_PATH = Path(self.tmp.name) / "test.sqlite3"
        self.client = TestClient(app.app)
    def tearDown(self): self.tmp.cleanup()
    def test_accept_and_duplicate_idempotent(self):
        e = {"v":1,"node":"tray-01","seq":1,"kind":"slot_mass_g","value":24.6,"unit":"g"}
        self.assertEqual(self.client.post("/events",json=e).status_code,200)
        self.assertTrue(self.client.post("/events",json=e).json()["duplicate"])
        self.assertEqual(len(self.client.get("/events").json()),1)
    def test_reject_invalid(self):
        for e in ({"v":2}, {"v":True,"node":"n","seq":1,"kind":"return_pressed","value":True,"unit":"bool"}, {"v":1,"node":"n","seq":-1,"kind":"return_pressed","value":True,"unit":"bool"}, {"v":1,"node":"n","seq":1,"kind":"slot_mass_g","value":float("nan"),"unit":"g"}, {"v":1,"node":"n","seq":1,"kind":"slot_mass_g","value":501,"unit":"g"}):
            self.assertEqual(self.client.post("/events",content=json.dumps(e,allow_nan=True),headers={"content-type":"application/json"}).status_code,422)
    def test_size_limit(self):
        r = self.client.post("/events",content=b" "*513)
        self.assertEqual(r.status_code,413)

if __name__ == "__main__": unittest.main()
