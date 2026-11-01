#!/usr/bin/env python3
"""Fit a transparent linear baseline to illustrative synthetic samples."""
import csv
from pathlib import Path

path = Path(__file__).with_name("sample.csv")
if not path.exists():
    raise SystemExit("Run generate.py first")
rows = list(csv.DictReader(path.open()))
if not rows:
    raise SystemExit("No training rows")
x = [float(r["hours_since_service"]) for r in rows]
y = [float(r["remaining_pct"]) for r in rows]
mx, my = sum(x)/len(x), sum(y)/len(y)
slope = sum((a-mx)*(b-my) for a,b in zip(x,y)) / sum((a-mx)**2 for a in x)
print(f"illustrative baseline: remaining_pct = {my-slope*mx:.2f} + ({slope:.4f} * hours)")
print("No held-out evaluation; synthetic-only output is not a validated prediction.")
