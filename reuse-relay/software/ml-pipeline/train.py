#!/usr/bin/env python3
"""Synthetic-data pipeline smoke test only; no field-quality claim."""
import csv, random
from pathlib import Path
random.seed(7)
rows=[(random.uniform(0,20000), int(random.random()<0.2)) for _ in range(500)]
out=Path(__file__).with_name('synthetic_presence.csv')
with out.open('w',newline='') as f:
    w=csv.writer(f, lineterminator='\n');w.writerow(['weight_change_mg','staff_review_label']);w.writerows(rows)
print(f"wrote {len(rows)} synthetic samples to {out.name}; no model trained")
