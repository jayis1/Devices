import csv
import random
from pathlib import Path

random.seed(7)
out = Path(__file__).with_name("synthetic.csv")
with out.open("w", newline="") as f:
    w = csv.writer(f); w.writerow(["day", "segment", "leq_cdb", "relatively_quieter"])
    for day in range(30):
        values = [random.randint(4500, 8500) for _ in range(4)]
        cutoff = sorted(values)[1]
        for segment, level in enumerate(values):
            w.writerow([day, segment, level, int(level <= cutoff)])
print(f"wrote synthetic demonstration data: {out}")
