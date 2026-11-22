"""Synthetic data plumbing smoke-test; not evidence of forecast performance."""
import csv, random
from pathlib import Path

def generate(path: Path, rows: int = 100, seed: int = 17):
    rng = random.Random(seed)
    with path.open('w', newline='') as f:
        w = csv.writer(f); w.writerow(['hour_utc', 'energy_wh', 'tariff_per_kwh'])
        for _ in range(rows):
            w.writerow([rng.randrange(24), rng.randrange(0, 12000), round(rng.uniform(.05, .60), 3)])

if __name__ == '__main__':
    import argparse
    p = argparse.ArgumentParser(); p.add_argument('--smoke', action='store_true'); p.add_argument('--output', default='/tmp/chargeplan-synthetic.csv')
    a = p.parse_args()
    if not a.smoke: p.error('only --smoke synthetic-data mode is implemented')
    generate(Path(a.output)); print(f'wrote deterministic synthetic fixture: {a.output}')
