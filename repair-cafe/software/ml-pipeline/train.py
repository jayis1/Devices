"""Illustrative synthetic-data baseline; not a trained operational model."""
import csv, random
from pathlib import Path
random.seed(7)
out=Path(__file__).with_name('synthetic_waits.csv')
with out.open('w',newline='') as f:
    w=csv.writer(f); w.writerow(['queue_position','open_benches','minutes_to_start'])
    for _ in range(500):
        q=random.randint(1,20); b=random.randint(0,5)
        w.writerow([q,b,max(0,round(q*12/(b+1)+random.gauss(0,5),1))])
print(f'Generated synthetic-only dataset: {out.name} (500 rows). No model training or field evaluation performed.')
