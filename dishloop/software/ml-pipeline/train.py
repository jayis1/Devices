"""Synthetic demonstration only: flag implausible count/mass pairs."""
import csv, random
from pathlib import Path
random.seed(7)
rows=[]
for i in range(100):
    count=random.randint(0,40)
    mass=count*35+random.gauss(0,60)
    rows.append((count,mass,int(mass > count*35+220)))
out=Path(__file__).with_name('synthetic.csv')
with out.open('w', newline='') as f:
    w=csv.writer(f); w.writerow(['return_count','mass_g','synthetic_flag']); w.writerows(rows)
print(f'Wrote {len(rows)} synthetic rows to {out}')
