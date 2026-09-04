from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from urllib import request

payloads = [
    {
        'node_id': 'inline-water-quality-1',
        'kind': 'water_quality',
        'metrics': {'ph': 6.21, 'turbidity_ntu': 4.6, 'orp_mv': 168, 'pressure_kpa': 398},
        'timestamp': datetime.now(timezone.utc).isoformat(),
    },
    {
        'node_id': 'weather-1',
        'kind': 'weather',
        'metrics': {'rain_mm': 34, 'soil_deep_pct': 42, 'dry_spell_days': 0},
        'timestamp': datetime.now(timezone.utc).isoformat(),
    },
]

for payload in payloads:
    body = json.dumps(payload).encode()
    req = request.Request('http://127.0.0.1:8094/api/v1/telemetry', data=body, headers={'Content-Type': 'application/json'})
    with request.urlopen(req, timeout=5) as resp:
        print(resp.read().decode())

Path(__file__).with_name('demo_output.json').write_text(json.dumps(payloads, indent=2))
