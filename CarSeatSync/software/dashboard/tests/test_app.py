import pathlib
import sys
import unittest

from fastapi.testclient import TestClient

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from app.main import app, store


class CarSeatSyncApiTest(unittest.TestCase):
    def setUp(self) -> None:
        store.events.clear()
        self.client = TestClient(app)

    def test_health(self) -> None:
        response = self.client.get('/health')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()['status'], 'ok')

    def test_trip_risk_flow(self) -> None:
        payload = {
            'child_id': 'child-7',
            'vehicle_id': 'van-2',
            'node': 'vehicle-hub',
            'ignition_on': False,
            'child_present': True,
            'buckle_closed': True,
            'caregiver_nearby': False,
            'cabin_temp_c': 40.2,
            'heat_slope_c_per_min': 0.41,
            'strap_tension_n': 22.0,
            'chest_clip_ratio': 0.40,
            'cry_score': 0.6,
            'bag_present': False,
        }
        post = self.client.post('/telemetry', json=payload)
        self.assertEqual(post.status_code, 200)

        summary = self.client.get('/risk/summary', params={'child_id': 'child-7'})
        self.assertEqual(summary.status_code, 200)
        self.assertEqual(summary.json()['risk_level'], 'red')

        rec = self.client.get('/recommendations', params={'child_id': 'child-7'})
        self.assertEqual(rec.status_code, 200)
        self.assertGreaterEqual(len(rec.json()['actions']), 2)

        timeline = self.client.get('/children/child-7/timeline')
        self.assertEqual(timeline.status_code, 200)
        self.assertEqual(len(timeline.json()), 1)


if __name__ == '__main__':
    unittest.main()
