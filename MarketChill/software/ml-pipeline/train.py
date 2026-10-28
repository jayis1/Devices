"""Synthetic demonstration only: threshold labels are not food-safety guidance."""
import csv
import random
from pathlib import Path

def generate(path: Path, n: int = 500) -> None:
    rng = random.Random(7)
    with path.open("w", newline="") as f:
        w = csv.writer(f); w.writerow(["temp_c", "minutes_above_configured_threshold", "excursion_label"])
        for _ in range(n):
            t = rng.uniform(-2, 12); m = rng.uniform(0, 180)
            w.writerow([round(t, 2), round(m, 1), int(t > 5 and m > 15)])

if __name__ == "__main__":
    output = Path("synthetic.csv"); generate(output)
    print(f"Generated synthetic example data: {output}")
    print("No model training or real-world safety evaluation is performed.")
