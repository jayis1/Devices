"""Synthetic demonstration only; no field-validated classifier is provided."""
import argparse
import random

def synthetic_demo(seed: int = 7) -> dict[str, int]:
    rng = random.Random(seed)
    rows = [(rng.randint(0, 600), rng.choice([0, 1])) for _ in range(100)]
    return {"synthetic_rows": len(rows), "seed": seed}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--synthetic", action="store_true")
    args = parser.parse_args()
    if args.synthetic:
        print(synthetic_demo())
    else:
        parser.error("No field dataset supplied; use --synthetic for smoke test")
