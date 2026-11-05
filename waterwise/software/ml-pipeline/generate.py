# Demonstration only: synthetic values validate file/feature plumbing, not accuracy.
import csv, random
from pathlib import Path
random.seed(7)
out = Path(__file__).with_name('synthetic_sessions.csv')
with out.open('w', newline='') as f:
    w = csv.writer(f); w.writerow(['flow_l_min','hot_wait_s','duration_s'])
    for _ in range(200):
        w.writerow([round(random.uniform(2,12),2), random.randint(0,90), random.randint(60,900)])
print(f'Wrote synthetic demonstration data to {out}')
