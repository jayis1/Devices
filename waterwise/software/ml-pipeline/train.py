# No model is trained: synthetic data is a pipeline smoke-test only.
import csv
from pathlib import Path
p = Path(__file__).with_name('synthetic_sessions.csv')
with p.open(newline='') as f:
    rows = list(csv.DictReader(f))
assert rows and all(float(r['flow_l_min']) >= 0 for r in rows)
print(f'Validated {len(rows)} synthetic rows; no performance claim.')
