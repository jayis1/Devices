"""Synthetic-only schema demo; never train or claim hazard performance from this data."""
import random

def synthetic_rows(n=100, seed=7):
    random.seed(seed)
    return [{"wet": random.random(), "tilt": random.random(), "label": int(random.random() > .8)} for _ in range(n)]

if __name__ == "__main__":
    rows = synthetic_rows()
    print(f"Generated {len(rows)} synthetic rows; no model fit or field metric is implied.")
