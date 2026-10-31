import os, tempfile, unittest, sys
from datetime import datetime, timezone
from fastapi.testclient import TestClient
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parents[1]))
from app import app

class ApiTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); os.environ['DRAINWATCH_DB']=self.tmp.name+'/test.db'
        import app as mod; mod.DB=__import__('pathlib').Path(os.environ['DRAINWATCH_DB'])
        self.client=TestClient(app)
    def tearDown(self): self.tmp.cleanup()
    def test_ingest_duplicate_and_query(self):
        p={'node_id':3,'sequence':1,'kind':'inlet','value':725,'received_at':datetime.now(timezone.utc).isoformat(),'valid':True}
        self.assertEqual(self.client.post('/events',json=p).status_code,200)
        self.assertTrue(self.client.post('/events',json=p).json()['duplicate'])
        self.assertEqual(self.client.get('/nodes').json()[0]['value'],725)
    def test_reject_range(self):
        p={'node_id':3,'sequence':1,'kind':'inlet','value':99999,'received_at':datetime.now(timezone.utc).isoformat()}
        self.assertEqual(self.client.post('/events',json=p).status_code,422)
if __name__=='__main__': unittest.main()
