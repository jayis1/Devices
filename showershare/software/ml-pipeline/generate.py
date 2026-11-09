"""Synthetic fixture baseline only; no trained or validated model is produced."""
import csv
import random

random.seed(7)
FIELDS = ["door_closed", "presence_band", "water_pulses", "label"]

def make_fixture(path, rows=100):
    with open(path, "w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        for _ in range(rows):
            closed = random.randint(0, 1)
            band = random.randint(0, 2)
            pulses = random.randint(0, 20)
            writer.writerow(dict(door_closed=closed, presence_band=band,
                                 water_pulses=pulses, label="unknown" if not closed else "inspect"))

if __name__ == "__main__":
    make_fixture("synthetic.csv")
