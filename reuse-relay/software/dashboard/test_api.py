import unittest
try:
    from fastapi.testclient import TestClient
    from main import app
except ImportError as exc:
    raise unittest.SkipTest(f"API test dependencies unavailable: {exc}")
class ApiTest(unittest.TestCase):
    def test_offer_lifecycle(self):
        c=TestClient(app)
        self.assertEqual(c.get('/health').json(), {'status':'ok'})
        item=c.post('/offers',json={'category':'books','expires_in_hours':1}).json()
        self.assertEqual(item['category'],'books')
        self.assertEqual(c.post(f"/offers/{item['id']}/picked-up").json()['status'],'picked_up')
    def test_reject_unknown_category(self):
        self.assertEqual(TestClient(app).post('/offers',json={'category':'secret'}).status_code,422)
if __name__ == '__main__': unittest.main()
