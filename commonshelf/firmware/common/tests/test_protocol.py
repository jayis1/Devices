import json, unittest, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "software" / "dashboard"))
from app import ingest, Reading

class ProtocolTests(unittest.TestCase):
    def test_roundtrip_shape(self):
        data = json.loads(json.dumps(dict(v=1,node="s1",seq=2,ts=0,kind="mass_g",value=1200,unit="g",battery_mv=3300)))
        self.assertTrue(ingest("demo", "s1", Reading(**data))["accepted"])
    def test_bounds_rejected(self):
        r = Reading(v=1,node="s1",seq=2,ts=0,kind="temp_c_x100",value=99999,unit="cC",battery_mv=3300)
        with self.assertRaises(Exception): ingest("demo", "s1", r)

if __name__ == "__main__": unittest.main()
