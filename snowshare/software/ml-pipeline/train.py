"""Synthetic-data baseline only; not a validated snow/ice classifier."""
import csv
import random
from pathlib import Path

def generate(path: Path, rows: int = 100, seed: int = 7) -> None:
    rng = random.Random(seed)
    with path.open('w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['site_id','event_id','surface_temp_c','rain_tips','inspection_label'])
        for i in range(rows):
            temp = rng.uniform(-12, 5)
            tips = rng.randrange(0, 8)
            # Synthetic labels are illustrative, not observations.
            label = int(temp < 0 and tips > 2)
            writer.writerow([f'site-{i%5}',f'event-{i//5}',round(temp,2),tips,label])

if __name__ == '__main__':
    out = Path(__file__).with_name('synthetic.csv')
    generate(out)
    print(f'generated synthetic example: {out}')
