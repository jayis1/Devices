"""Synthetic contract fixture only; not training data and no model is produced."""
import csv
from pathlib import Path

out = Path(__file__).with_name('synthetic_readiness.csv')
rows = [
    {'node_id':'fixture-01','revision':'demo-1','fault_count':0,'ready_label':1},
    {'node_id':'fixture-02','revision':'demo-1','fault_count':1,'ready_label':0},
]
with out.open('w', newline='', encoding='utf-8') as f:
    writer=csv.DictWriter(f, fieldnames=rows[0].keys(), lineterminator='\n'); writer.writeheader(); writer.writerows(rows)
print(f'Wrote {len(rows)} synthetic rows to {out}')
