import unittest
from fastapi.testclient import TestClient
from app import app, latest

class ApiTests(unittest.TestCase):
    def setUp(self):
        latest.clear()
        self.client = TestClient(app)

    def test_ingest_and_read(self):
        r = self.client.post('/v1/readings', json={'node':'a1b2c3d4','kind':'room','seq':1,'ts':2000000000,'data':{'lux':450}})
        self.assertEqual(r.status_code, 202)
        body = self.client.get('/v1/readings').json()
        self.assertEqual(body['a1b2c3d4']['data']['lux'], 450)
        self.assertFalse(body['a1b2c3d4']['stale'])

    def test_bounds_and_replay_rejected(self):
        bad = self.client.post('/v1/readings', json={'node':'a1b2c3d4','kind':'room','seq':1,'ts':1,'data':{'lux':200000}})
        self.assertEqual(bad.status_code, 422)
        payload = {'node':'a1b2c3d4','kind':'shade','seq':7,'ts':1,'data':{}}
        self.assertEqual(self.client.post('/v1/readings', json=payload).status_code, 202)
        self.assertEqual(self.client.post('/v1/readings', json=payload).status_code, 409)

if __name__ == '__main__': unittest.main()
