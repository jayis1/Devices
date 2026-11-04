# Placeholder baseline: reports median remaining-time target; no model trained.
import csv

with open("synthetic.csv", newline="") as f:
    rows = list(csv.DictReader(f))
if not rows:
    raise SystemExit("run generate.py first")
ys = sorted(int(r["remaining_min"]) for r in rows)
print(f"rows={len(ys)} synthetic_median_remaining_min={ys[len(ys)//2]}")
print("No predictive performance claim; collect consented real sessions and split by household/site before evaluation.")
