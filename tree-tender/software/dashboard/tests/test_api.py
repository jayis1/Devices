import unittest
import sys
from pathlib import Path
from fastapi.testclient import TestClient
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from main import app

class ApiTests(unittest.TestCase):
    def setUp(self): self.client=TestClient(app)
    def test_ingest_and_read(self):
        payload={'v':1,'node':'probe-test','seq':7,'ts':100,'kind':'soil','soil_pct':35.5,'temp_c':12.0,'rh_pct':60}
        self.assertEqual(self.client.post('/sites/test/observations',json=payload).status_code,200)
        self.assertEqual(self.client.get('/sites/test/observations').json()[0]['soil_pct'],35.5)
    def test_reject_missing_soil(self):
        payload={'v':1,'node':'probe-test','seq':8,'ts':100,'kind':'soil'}
        self.assertEqual(self.client.post('/sites/test/observations',json=payload).status_code,422)

if __name__=='__main__': unittest.main()
