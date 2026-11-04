import unittest
from main import validate_event

class EventTests(unittest.TestCase):
    def test_accepts_valid_event(self):
        self.assertTrue(validate_event({"v": 1, "node": "button-a", "seq": 1, "type": "task_done", "value": 1, "unit": "bool", "utc_ms": 0}))
    def test_rejects_bad_version_range_and_units(self):
        base = {"v": 1, "node": "dock-a", "seq": 2, "type": "supply_g", "value": 42, "unit": "g", "utc_ms": 0}
        self.assertFalse(validate_event({**base, "v": 2}))
        self.assertFalse(validate_event({**base, "value": 100001}))
        self.assertFalse(validate_event({**base, "unit": "kg"}))
    def test_rejects_invalid_availability_and_node(self):
        base = {"v": 1, "node": "beacon", "seq": 3, "type": "availability", "value": 3, "unit": "enum", "utc_ms": 0}
        self.assertFalse(validate_event(base))
        self.assertFalse(validate_event({**base, "node": ""}))

if __name__ == "__main__":
    unittest.main()
