import pathlib
import sys
import unittest

from fastapi.testclient import TestClient

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from app.main import app, store


class SickDaySyncApiTest(unittest.TestCase):
    def setUp(self) -> None:
        store.events.clear()
        self.client = TestClient(app)

    def test_health(self) -> None:
        response = self.client.get('/health')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()['status'], 'ok')

    def test_patient_flow(self) -> None:
        payload = {
            'patient_id': 'kid-1',
            'node': 'room-sentinel',
            'room_id': 'bedroom-a',
            'fever_c': 38.4,
            'spo2': 95.0,
            'resting_hr': 102.0,
            'coughs_per_hour': 37,
            'co2_ppm': 1580,
            'humidity_pct': 33.0,
            'hydration_ml': 220,
        }
        post = self.client.post('/telemetry', json=payload)
        self.assertEqual(post.status_code, 200)

        summary = self.client.get('/risk/summary', params={'patient_id': 'kid-1'})
        self.assertEqual(summary.status_code, 200)
        self.assertEqual(summary.json()['patient_id'], 'kid-1')
        self.assertIn(summary.json()['risk_level'], {'amber', 'red'})

        rec = self.client.get('/recommendations', params={'patient_id': 'kid-1'})
        self.assertEqual(rec.status_code, 200)
        self.assertTrue(len(rec.json()['actions']) >= 1)

        timeline = self.client.get('/patients/kid-1/timeline')
        self.assertEqual(timeline.status_code, 200)
        self.assertEqual(len(timeline.json()), 1)


if __name__ == '__main__':
    unittest.main()
