import os
import tempfile
import unittest
from fastapi.testclient import TestClient
from app import app

class ApiTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        import app as module
        self.old = module.DB
        module.DB = __import__('pathlib').Path(self.tmp.name) / 'test.sqlite3'
        self.client = TestClient(app)
    def tearDown(self):
        import app as module
        module.DB = self.old
        self.tmp.cleanup()
    def test_health_and_schema(self):
        self.assertEqual(self.client.get('/health').status_code, 200)
        event = dict(v=1,type='ready',node_id='art-01',seq=1,event_id='00112233445566778899aabbccddeeff',content_rev='r1',faults=[])
        self.assertEqual(self.client.post('/readiness',json=event).json()['accepted'], True)
        self.assertTrue(self.client.post('/readiness',json=event).json()['duplicate'])
        event['v']=2
        self.assertEqual(self.client.post('/readiness',json=event).status_code,422)
