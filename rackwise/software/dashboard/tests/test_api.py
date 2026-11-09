import unittest
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fastapi.testclient import TestClient
from main import app

class ApiTests(unittest.TestCase):
    def test_health(self):
        self.assertEqual(TestClient(app).get('/health').json(), {'status': 'ok'})
    def test_unknown_is_stale(self):
        response = TestClient(app).get('/v1/sites/demo/availability')
        self.assertEqual(response.status_code, 200)
        self.assertTrue(response.json()['stale'])
        self.assertIsNone(response.json()['free_bays'])

if __name__ == '__main__':
    unittest.main()
