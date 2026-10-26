"""Synthetic demonstration data only; no field-data performance claim."""
import random

def sample(seed=7, n=100):
    rng = random.Random(seed)
    return [(rng.uniform(0, 300), rng.uniform(0, 30000), int(rng.random() < .1)) for _ in range(n)]

if __name__ == "__main__":
    rows = sample()
    print(f"synthetic_rows={len(rows)}; labels are illustrative; model not trained/validated")
