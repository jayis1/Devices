from __future__ import annotations

import json
from pathlib import Path

sample = {
    'patient_id': 'demo-child',
    'risk_level': 'amber',
    'spread_score': 61.2,
    'hydration_risk': 43.8,
    'fever_forecast': 38.6,
    'actions': [
        'Increase fresh-air exchange or HEPA mode in the assigned sick room.',
        'Offer fluids now and recheck intake within 30 minutes.'
    ]
}
path = Path(__file__).resolve().parent / 'demo_output.json'
path.write_text(json.dumps(sample, indent=2) + "\n")
print(path)
