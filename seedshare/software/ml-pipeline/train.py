import csv
from pathlib import Path
p = Path(__file__).with_name('synthetic.csv')
rows = list(csv.DictReader(p.open()))
cut = int(len(rows) * .8)
train, test = rows[:cut], rows[cut:]
predict = lambda r: int(float(r['rh_pct']) >= 65.0)
correct = sum(predict(r) == int(r['high_rh_advisory']) for r in test)
print(f'synthetic threshold baseline accuracy={correct/len(test):.3f}; holdout={len(test)}')
print('Not a validated seed viability model; live advisory remains threshold-based.')
