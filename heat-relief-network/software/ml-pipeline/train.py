import argparse
import csv
import random
from pathlib import Path

FEATURES = ("temp_c", "rh_pct", "co2_ppm", "water_g", "crossings")

def generate(path: Path, count: int, seed: int) -> None:
    rng = random.Random(seed)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=[*FEATURES, "label"])
        w.writeheader()
        for _ in range(count):
            temp = rng.uniform(19, 39)
            rh = rng.uniform(20, 85)
            co2 = rng.randint(420, 2400)
            water = rng.randint(0, 8000)
            crossings = rng.randint(0, 120)
            label = int(temp > 32 or co2 > 1800 or water < 500)
            w.writerow(dict(temp_c=round(temp, 2), rh_pct=round(rh, 2), co2_ppm=co2, water_g=water, crossings=crossings, label=label))

def main() -> None:
    p = argparse.ArgumentParser(description="Generate labeled synthetic readiness examples; not field data.")
    p.add_argument("--synthetic", action="store_true", help="required; this demo has no real dataset")
    p.add_argument("--output", type=Path, default=Path("synthetic_readiness.csv"))
    p.add_argument("--count", type=int, default=500)
    p.add_argument("--seed", type=int, default=7)
    a = p.parse_args()
    if not a.synthetic:
        p.error("pass --synthetic; this pipeline only generates synthetic data")
    if a.count < 1 or a.count > 100000:
        p.error("count must be 1..100000")
    generate(a.output, a.count, a.seed)
    print(f"wrote {a.count} synthetic rows to {a.output}")

if __name__ == "__main__":
    main()
