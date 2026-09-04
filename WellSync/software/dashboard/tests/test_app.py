import unittest

from fastapi.testclient import TestClient

from main import app


class WellSyncApiTests(unittest.TestCase):
    def setUp(self) -> None:
        self.client = TestClient(app)
        self.client.post('/api/v1/reset')

    def test_health(self) -> None:
        response = self.client.get('/api/v1/health')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()['status'], 'ok')

    def test_overview_rises_to_treat(self) -> None:
        self.client.post('/api/v1/telemetry', json={
            'node_id': 'inline-water-quality-1',
            'kind': 'water_quality',
            'metrics': {'ph': 6.2, 'turbidity_ntu': 4.8, 'orp_mv': 170},
            'timestamp': '2026-09-04T00:00:00Z',
        })
        self.client.post('/api/v1/telemetry', json={
            'node_id': 'weather-1',
            'kind': 'weather',
            'metrics': {'rain_mm': 32, 'soil_deep_pct': 41, 'dry_spell_days': 0},
            'timestamp': '2026-09-04T00:01:00Z',
        })
        response = self.client.get('/api/v1/overview')
        body = response.json()
        self.assertEqual(response.status_code, 200)
        self.assertIn(body['state'], {'treat', 'do_not_drink'})
        self.assertGreater(body['risks']['contamination'], 0.58)

    def test_service_log(self) -> None:
        response = self.client.post('/api/v1/service-log', json={
            'action': 'filter_change',
            'performed_by': 'homeowner',
            'notes': 'replaced under-sink carbon block',
            'timestamp': '2026-09-04T00:02:00Z',
        })
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()['count'], 1)


if __name__ == '__main__':
    unittest.main()
