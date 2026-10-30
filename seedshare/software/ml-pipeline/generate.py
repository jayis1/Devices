import csv
import random
from pathlib import Path
random.seed(7)
rows = []
for _ in range(200):
    rh = random.uniform(25, 80)
    temp = random.uniform(8, 32)
    # Transparent synthetic label, not a seed viability label.
    advisory = int(rh >= 65)
    rows.append((rh, temp, advisory))
path = Path(__file__).with_name('synthetic.csv')
with path.open('w', newline='') as f:
    w = csv.writer(f); w.writerow(['rh_pct','temp_c','high_rh_advisory']); w.writerows(rows)
print(f'wrote {len(rows)} synthetic rows to {path}')
