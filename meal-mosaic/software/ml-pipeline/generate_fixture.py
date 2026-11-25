"""Generate illustrative observations; synthetic output is not operational data."""
import csv, random
from pathlib import Path
out=Path(__file__).with_name('sample.csv')
r=random.Random(17)
with out.open('w',newline='') as f:
 w=csv.writer(f); w.writerow(['site','node','kind','value','status']);
 for i in range(120): w.writerow(['demo',f'node-{i%3}',1+i%3,round(r.gauss(2200,250)),0])
print(f'wrote {out}')
