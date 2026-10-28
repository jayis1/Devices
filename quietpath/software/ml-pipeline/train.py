import csv
from pathlib import Path
p = Path(__file__).with_name("synthetic.csv")
with p.open(newline="") as f:
    rows = list(csv.DictReader(f))
if not rows:
    raise SystemExit("no data: run generate_synthetic.py first")
# Sanity-check the synthetic label distribution; this is not model training.
positive = sum(int(r["relatively_quieter"]) for r in rows)
print(f"synthetic rows={len(rows)} relatively_quieter_labels={positive}; no model fitted or evaluated")
