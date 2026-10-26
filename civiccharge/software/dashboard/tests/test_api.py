import unittest
from app.main import app, Outlet, update

class ApiTests(unittest.TestCase):
    def test_model_bounds(self):
        with self.assertRaises(ValueError):
            Outlet(node_id="n1", voltage_v_rms=999, current_ma=0, temp_c=20, flags=1)

    def test_health_route_registered(self):
        self.assertIn("/health", [r.path for r in app.routes])

if __name__ == "__main__": unittest.main()
