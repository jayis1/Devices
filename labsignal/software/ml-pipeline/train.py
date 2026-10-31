"""Synthetic smoke-data generator; never evidence of real-world performance."""
import csv
import random
from pathlib import Path
random.seed(7)
with Path("synthetic_events.csv").open("w", newline="") as f:
    w = csv.writer(f); w.writerow(["value_milli", "label"])
    for _ in range(100):
        value = random.randint(0, 5000)
        w.writerow([value, "check" if value > 4000 else "normal"])
print("wrote 100 SYNTHETIC rows; no model trained or accuracy claimed")
