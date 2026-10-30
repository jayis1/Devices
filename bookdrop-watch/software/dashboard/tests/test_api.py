from unittest import TestCase
from pathlib import Path
import sys
from fastapi.testclient import TestClient
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from main import app

class ApiTests(TestCase):
    def test_health_and_validation(self):
        client = TestClient(app)
        self.assertEqual(client.get('/health').status_code, 200)
        self.assertEqual(client.post('/telemetry', json={'v':1,'node':'chute-01','seq':1,'kind':'passage','value':1}).status_code, 200)
        self.assertEqual(client.post('/telemetry', json={'v':1,'node':'x','seq':1,'kind':'remote','value':1}).status_code, 400)
