import unittest
from fastapi.testclient import TestClient
from main import app

class ApiTest(unittest.TestCase):
    def setUp(self):
        from main import _events, _seen
        _events.clear(); _seen.clear()
        self.client = TestClient(app)
    def test_health(self):
        self.assertEqual(self.client.get('/health').json()['status'], 'ok')
    def test_valid_and_duplicate(self):
        event = dict(v=1,node='dock-01',seq=7,ts=1730000000,kind='deposit',mass_g=230)
        self.assertTrue(self.client.post('/events',json=event).json()['accepted'])
        self.assertTrue(self.client.post('/events',json=event).json()['duplicate'])
    def test_bounds(self):
        event = dict(v=1,node='dock-01',seq=8,ts=1,kind='deposit',mass_g=30000)
        self.assertEqual(self.client.post('/events',json=event).status_code,422)

if __name__ == '__main__': unittest.main()
