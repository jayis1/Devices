import unittest
from app import health, status, events
class ApiTests(unittest.TestCase):
    def test_health(self): self.assertEqual(health(), {"status":"ok"})
    def test_unknown_is_not_available(self): self.assertEqual(status("demo")["status"], "unknown")
    def test_empty_events(self): self.assertEqual(events("demo")["events"], [])
if __name__ == "__main__": unittest.main()
