import unittest
try:
    from fastapi.testclient import TestClient
    from app import app
except ImportError:
    TestClient = None

@unittest.skipIf(TestClient is None, "FastAPI/TestClient dependencies unavailable")
class ApiSmokeTest(unittest.TestCase):
    def test_health_and_event_validation(self):
        client = TestClient(app)
        self.assertEqual(client.get('/health').status_code, 200)
        self.assertEqual(client.post('/events', json={'v':1,'kind':2,'seq':1,'value_milli':3}).status_code, 202)
        self.assertEqual(client.post('/events', json={'v':9,'kind':2,'seq':1,'value_milli':3}).status_code, 422)

if __name__ == '__main__':
    unittest.main()
