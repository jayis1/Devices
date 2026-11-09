"""Synthetic-only baseline scaffold; output is not a real demand model."""
import random


def synthetic_data(seed=7, n=100):
    rng = random.Random(seed)
    return [(rng.randrange(0, 9), rng.randrange(0, 24), rng.randrange(0, 2)) for _ in range(n)]


def evaluate(rows):
    # Baseline: predict free-bay category from synthetic row's first field.
    correct = sum(("full" if free == 0 else "available") == ("full" if free == 0 else "available") for free, _hour, _rain in rows)
    return correct / len(rows) if rows else 0.0

if __name__ == "__main__":
    rows = synthetic_data()
    print(f"synthetic rows={len(rows)} baseline_self_consistency={evaluate(rows):.3f}; not a field metric")
