import tempfile
import unittest
from pathlib import Path
import app
class ApiTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); app.DB=Path(self.tmp.name)/"test.db"
    def tearDown(self): self.tmp.cleanup()
    def test_put_and_search(self):
        result=app.put_box("box-123456", app.Box(box_id="box-123456",contents="kettle and mugs",room="Kitchen"))
        self.assertEqual(result["room"],"Kitchen")
        found=app.search("kettle")
        self.assertEqual(found["results"][0]["box_id"],"box-123456")
if __name__=="__main__": unittest.main()
