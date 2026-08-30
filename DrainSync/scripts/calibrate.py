from __future__ import annotations

import json
from pathlib import Path

profile = {
    'home_id': 'demo-home',
    'nodes': [
        {'node_id': 'sink-1', 'zone': 'kitchen', 'branch_id': 'kitchen-west', 'baseline_duration_ms': 910},
        {'node_id': 'floor-1', 'zone': 'basement', 'trap_depth_raw_nominal': 420},
        {'node_id': 'stack-1', 'zone': 'service', 'level_mm_idle': 690},
        {'node_id': 'act-1', 'zone': 'service', 'open_pct': 0, 'closed_pct': 100}
    ]
}

out = Path(__file__).resolve().parent / 'calibration_profile.json'
out.write_text(json.dumps(profile, indent=2), encoding='utf-8')
print(out)
