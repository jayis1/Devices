#!/usr/bin/env python3
"""Generate illustrative synthetic bag-level samples; not field data."""
import csv
import random
from pathlib import Path

random.seed(7)
with Path(__file__).with_name("sample.csv").open("w", newline="") as f:
    out = csv.writer(f)
    out.writerow(["hours_since_service", "remaining_pct"])
    for _ in range(200):
        hours = random.randint(0, 168)
        out.writerow([hours, max(0, min(100, round(100 - hours * random.uniform(.2, .8))))])
