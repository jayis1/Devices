import unittest
try:
    from fastapi.testclient import TestClient
    from app import app
    client = TestClient(app)
except ImportError:
    client = None

@unittest.skipIf(client is None, "FastAPI/test client dependencies unavailable")
class ApiTests(unittest.TestCase):
    def test_ingest_and_deduplicate(self):
        self.assertEqual(client.post('/v1/readings', json={'node_id':'tag-1','kind':'motion','sequence':1,'value':3}).status_code, 200)
        self.assertEqual(client.post('/v1/readings', json={'node_id':'tag-1','kind':'motion','sequence':1,'value':3}).status_code, 409)
    def test_reject_invalid_identifier(self):
        self.assertEqual(client.post('/v1/readings', json={'node_id':'bad/id','kind':'motion','sequence':1,'value':3}).status_code, 422)

if __name__ == '__main__': unittest.main()
