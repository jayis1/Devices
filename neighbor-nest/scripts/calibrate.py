import json
from pathlib import Path

# Calibration helper: supply two ADC readings and write per-probe endpoints.
import argparse
p = argparse.ArgumentParser()
p.add_argument('--dry', type=int, required=True)
p.add_argument('--wet', type=int, required=True)
p.add_argument('--probe', required=True)
p.add_argument('--output', default='calibration.json')
a = p.parse_args()
if a.dry == a.wet:
    p.error('dry and wet endpoints must differ')
Path(a.output).write_text(json.dumps({'probe': a.probe, 'dry_adc': a.dry, 'wet_adc': a.wet}, indent=2) + '\n')
