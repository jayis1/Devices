# Synthetic baseline only; not trained for real-world use.
import csv, random
from pathlib import Path
random.seed(7)
out = Path('synthetic_events.csv')
with out.open('w', newline='') as f:
    w = csv.writer(f); w.writerow(['delta_g','door_open','label'])
    for _ in range(200):
        delta = random.gauss(0, 25); door = random.randint(0, 1)
        w.writerow([round(delta, 2), door, int(abs(delta) > 80)])
print(f'wrote {out} (synthetic, not evaluation data)')
