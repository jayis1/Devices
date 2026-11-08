#!/usr/bin/env python3
"""Generate and fit a deterministic synthetic demo only; not real-world evidence."""
import csv
import random
from pathlib import Path

random.seed(7)
rows = []
for household in range(20):
    returned = random.randint(0, 12)
    cells = random.randint(4, 20)
    days = max(0, 14 - returned)
    rows.append((household, cells, returned, days))
out = Path(__file__).with_name("synthetic_demo.csv")
with out.open("w", newline="\n", encoding="utf-8") as f:
    writer = csv.writer(f, lineterminator="\n")
    writer.writerow(("demo_household", "cells_seen", "returned_count", "illustrative_ready_days"))
    writer.writerows(rows)
# Baseline demonstration: mean readiness days. This is not a trained or validated model.
mean = sum(r[3] for r in rows) / len(rows)
print(f"synthetic_rows={len(rows)} baseline_mean_days={mean:.2f}; not predictive evidence")
